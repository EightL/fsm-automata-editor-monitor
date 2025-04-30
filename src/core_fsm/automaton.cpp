// automaton.cpp
#include "automaton.hpp"
#include "../../external/nlohmann/json.hpp"

using namespace core_fsm;

namespace {
    // Helper: grab current variable‐values into a map for GuardCtx
    static std::unordered_map<std::string, Value>
    makeVarSnapshot(std::unordered_map<std::string, Variable> const& vars) {
        std::unordered_map<std::string, Value> snap;
        snap.reserve(vars.size());
        for (auto const& kv : vars) {
            snap.emplace(kv.first, kv.second.value());
        }
        return snap;
    }
}

static nlohmann::json jsonFromValue(const core_fsm::Value &v) {
    return std::visit([](auto&& x) -> nlohmann::json { return x; }, v);
}

void Automaton::addVariable(const Variable& var) {
    m_vars.emplace(var.name(), var);
}

void Automaton::addState(const State& s, bool initial) {
    m_states.push_back(s);
    if (m_states.size() == 1 || initial) {
        m_active = m_states.size() - 1;
    }
}

void Automaton::addTransition(const Transition& t) {
    m_transitions.push_back(t);
}

void Automaton::injectInput(const std::string& name,
                            const std::string& value)
{
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_incoming.emplace(name, value);
    }
    m_cv.notify_one();
}

void Automaton::requestStop() noexcept {
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_stop = true;
    }
    m_cv.notify_one();
}

const std::string& Automaton::currentState() const noexcept {
    return m_states[m_active].name();
}

const std::vector<Automaton::EventLog>&
Automaton::log() const noexcept
{
    return m_log;
}

void Automaton::run() {
    while (true) {
        // 1) Fire all immediate (delay == 0) unconditional transitions
        while (true) {
            auto varSnap = makeVarSnapshot(m_vars);
            GuardCtx guardCtx{varSnap, m_inputs};  // renamed from ctx
            bool didImmediate = false;
            for (size_t i = 0; i < m_transitions.size(); ++i) {
                const auto& t = m_transitions[i];
                if (t.src() == m_active
                    && !t.isDelayed()
                    && t.isTriggered("", guardCtx))  // use guardCtx here
                {
                    m_active = t.dst();
                    m_log.push_back({ Clock::now(), m_states[m_active].name(), "", "" });
                    if (m_snapshotHook) m_snapshotHook();
                    // ─── BROADCAST to attached channel ────────────────────────────────────────────
                    if (m_channel) {
                        nlohmann::json j = {
                        {"type",    "state"},
                        {"seq",     ++m_seq},
                        {"ts",      std::chrono::duration_cast<
                                        std::chrono::milliseconds>(
                                        Clock::now().time_since_epoch()
                                    ).count()},
                        {"state",   m_states[m_active].name()},
                        {"inputs",  m_inputs},
                        {"vars",    [&]{
                            nlohmann::json snapshot;
                            for (auto &kv : m_vars)
                                snapshot[kv.first] = jsonFromValue(kv.second.value());
                            return snapshot;
                        }()},
                        {"outputs", m_outputs}
                        };
                        m_channel->send({ j.dump() });
                    }
                    // ──────────────────────────────────────────────────────────────────────────────

                    m_stateSince = Clock::now();
                    // Create a snapshot of variable values
                    auto varSnap = makeVarSnapshot(m_vars);
                    Context ctx{ varSnap, m_inputs, m_outputs, m_stateSince };
                    m_states[m_active].onEnter(ctx);
                    
                    didImmediate = true;
                    break;
                }
            }
            if (!didImmediate) break;
        }

        // 2) Schedule all delayed (delay > 0) unconditional transitions once
        {
            auto varSnap = makeVarSnapshot(m_vars);
            GuardCtx guardCtx{varSnap, m_inputs};  // renamed from ctx
            for (size_t i = 0; i < m_transitions.size(); ++i) {
                const auto& t = m_transitions[i];
                if (t.src() == m_active
                    && t.isDelayed()
                    && t.isTriggered("", guardCtx))  // use guardCtx here
                {
                    m_timers.push(
                        Pending{ Clock::now() + t.delay(), i }
                    );
                }
            }
        }

        // 3) Purge timers from non‐active states
        while (!m_timers.empty()) {
            auto const& top = m_timers.top();
            if (m_transitions[top.transitionIndex].src() != m_active)
                m_timers.pop();
            else
                break;
        }

        if (m_channel) {
            io_bridge::Packet p;
            while (m_channel->poll(p)) {
                auto j = nlohmann::json::parse(p.json, nullptr, false);
                if (!j.is_discarded() && j.value("type","") == "inject") {
                    injectInput(
                      j.at("name").get<std::string>(),
                      j.at("value").get<std::string>()
                    );
                }
                else if (!j.is_discarded() && j.value("type","") == "shutdown") {
                    requestStop();
                }
            }
        }

        // 4) Block until either new input, timer or stop
        std::unique_lock<std::mutex> lk(m_mtx);
        if (!m_stop && m_incoming.empty()) {
            if (m_timers.empty()) {
                m_cv.wait(lk, [&]{ return m_stop || !m_incoming.empty(); });
            } else {
                auto nextDue = m_timers.top().due;
                m_cv.wait_until(lk, nextDue, [&]{ 
                    return m_stop || !m_incoming.empty(); 
                });
            }
        }

        // 5) Check for stop request
        if (m_stop) return;

        // 6) Handle one input event, if any
        if (!m_incoming.empty()) {
            auto [iname, ival] = std::move(m_incoming.front());
            m_incoming.pop();
            lk.unlock();

            // update last‐known input
            m_inputs[iname] = ival;

            // evaluate transitions triggered by this input
            auto varSnap = makeVarSnapshot(m_vars);
            GuardCtx guardCtx{varSnap, m_inputs};  // renamed from ctx

            bool didTransition = false;
            for (std::size_t i = 0; i < m_transitions.size(); ++i) {
                auto const& t = m_transitions[i];
                if (t.src() != m_active
                    || !t.isTriggered(iname, guardCtx))  // use guardCtx here
                    continue;

                if (!t.isDelayed()) {
                    m_active = t.dst();
                    m_log.push_back({ Clock::now(), m_states[m_active].name(), "", "" });
                    if (m_snapshotHook) m_snapshotHook();
                    // ─── BROADCAST to attached channel ────────────────────────────────────────────
                    if (m_channel) {
                        nlohmann::json j = {
                        {"type",    "state"},
                        {"seq",     ++m_seq},
                        {"ts",      std::chrono::duration_cast<
                                        std::chrono::milliseconds>(
                                        Clock::now().time_since_epoch()
                                    ).count()},
                        {"state",   m_states[m_active].name()},
                        {"inputs",  m_inputs},
                        {"vars",    [&]{
                            nlohmann::json snapshot;
                            for (auto &kv : m_vars)
                                snapshot[kv.first] = jsonFromValue(kv.second.value());
                            return snapshot;
                        }()},
                        {"outputs", m_outputs}
                        };
                        m_channel->send({ j.dump() });
                    }
                    // ──────────────────────────────────────────────────────────────────────────────

                    m_stateSince = Clock::now();
                    // Create a snapshot of variable values
                    auto varSnap = makeVarSnapshot(m_vars);
                    Context ctx{ varSnap, m_inputs, m_outputs, m_stateSince };
                    m_states[m_active].onEnter(ctx);
                    
                    didTransition = true;
                } else {
                    m_timers.push(
                        Pending{ Clock::now() + t.delay(), i }
                    );
                }
                if (didTransition) break;
            }
            // loop again to pick up any new unconditional transitions
            continue;
        }

        // 7) Otherwise a timer must have fired
        lk.unlock();
        auto now = Clock::now();
        if (!m_timers.empty() && m_timers.top().due <= now) {
            auto p = m_timers.top();
            m_timers.pop();

            // only fire if still in the same source state
            if (m_transitions[p.transitionIndex].src() == m_active) {
                auto const& t = m_transitions[p.transitionIndex];
                auto varSnap = makeVarSnapshot(m_vars);
                GuardCtx guardCtx{varSnap, m_inputs};  // renamed from ctx

                m_active = t.dst();
                m_log.push_back({ Clock::now(), m_states[m_active].name(), "", "" });
                if (m_snapshotHook) m_snapshotHook();
                // ─── BROADCAST to attached channel ────────────────────────────────────────────
                if (m_channel) {
                    nlohmann::json j = {
                    {"type",    "state"},
                    {"seq",     ++m_seq},
                    {"ts",      std::chrono::duration_cast<
                                    std::chrono::milliseconds>(
                                    Clock::now().time_since_epoch()
                                ).count()},
                    {"state",   m_states[m_active].name()},
                    {"inputs",  m_inputs},
                    {"vars",    [&]{
                        nlohmann::json snapshot;
                        for (auto &kv : m_vars)
                            snapshot[kv.first] = jsonFromValue(kv.second.value());
                        return snapshot;
                    }()},
                    {"outputs", m_outputs}
                    };
                    m_channel->send({ j.dump() });
                }
                // ──────────────────────────────────────────────────────────────────────────────

                m_stateSince = Clock::now();
                // Create a snapshot of variable values
                auto contextVarSnap = makeVarSnapshot(m_vars);  // Renamed to avoid redefinition
                Context ctx{ contextVarSnap, m_inputs, m_outputs, m_stateSince };
                m_states[m_active].onEnter(ctx);
                
            }
        }
        // then loop back to step 1
    }
}