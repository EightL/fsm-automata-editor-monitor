// automaton.cpp
#include "automaton.hpp"
#include "scheduler.hpp"
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>
#include <iostream>
#include <QDebug>
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

void Automaton::broadcastSnapshot() {
    if (!m_channel) return;
    
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
    std::cerr << "RUNTIME → UDP: " << j.dump() << std::endl;
}

bool Automaton::fireTransition(size_t transitionIndex,
                               const std::string& triggerName)
{
    const auto& t = m_transitions[transitionIndex];
    if (t.src() != m_active) return false;

    // 1) remember old state
    const auto oldState = m_active;

    // 2) actually change
    m_active = t.dst();
    // push_back via emplace to avoid ambiguous initializer-list
    m_log.emplace_back(
        Clock::now(),
        m_states[m_active].name(),
        triggerName,
        std::string{}      // the "extra" field
    );
    if (m_snapshotHook) m_snapshotHook();

    // 3) drop timers for *this* new state
    scheduler_.purgeForState(
        m_active,
        [&](size_t idx){ return m_transitions[idx].src(); }
    );

    // 4) only reset the clock if the state really changed
    if (m_active != oldState) {
        m_stateSince = Clock::now();
    }

    // 5) pass the (possibly unchanged) m_stateSince into the new Context
    Context ctx{ m_vars, m_inputs, m_outputs, m_stateSince };
    m_states[m_active].onEnter(ctx);
    
    m_inputs.clear();
    return true;
}


bool Automaton::processImmediateTransitions(const std::string& trigger) {
    bool anyFired = false;
    bool fired;
    
    do {
        fired = false;
        auto varSnap = makeVarSnapshot(m_vars);
        GuardCtx guardCtx{varSnap, m_inputs};
        
        for (size_t i = 0; i < m_transitions.size(); ++i) {
            const auto& t = m_transitions[i];
            if (t.src() == m_active && 
                t.isTriggered(trigger.empty() ? "" : trigger, guardCtx)) {
                
                // Compute delay: default 1ms
                auto effectiveDelay = std::chrono::milliseconds(1);

                if (t.hasVariableDelay()) {
                    // lookup the *current* variable value
                    const auto& varName = t.variableDelayName();
                    auto it = m_vars.find(varName);
                    if (it != m_vars.end()) {
                        switch (it->second.type()) {
                          case Variable::Type::Int:
                            effectiveDelay = std::chrono::milliseconds(
                                std::get<int>(it->second.value()));
                            break;
                          case Variable::Type::Double:
                            effectiveDelay = std::chrono::milliseconds(
                                static_cast<int>(std::get<double>(it->second.value())));
                            break;
                          default:
                            break;  // leave as 1ms or whatever you choose
                        }
                    }
                }
                else if (t.isDelayed()) {
                    // fixed‐delay transition
                    effectiveDelay = t.delay();
                }
                qDebug() << "[arm]" << m_transitions[i].src() << "→" 
                << m_transitions[i].dst()
                << "delay =" << effectiveDelay.count() << "ms";
                scheduler_.arm(i, effectiveDelay);

            }
        }
    } while (fired);
    
    return anyFired;
}

void Automaton::setVariable(const std::string& name,
                            const std::string& valueStr) noexcept
{
    // Look up the declared variable
    auto it = m_vars.find(name);
    if (it == m_vars.end())
        return;   // unknown variable, ignore

    // Parse the incoming string into the same variant type we stored at load time
    const auto varType = it->second.type();
    core_fsm::Value newVal;

    try {
        switch (varType) {
            case Variable::Type::Int:
                newVal = std::stoi(valueStr);
                break;
            case Variable::Type::Double:
                newVal = std::stod(valueStr);
                break;
            case Variable::Type::String:
                newVal = valueStr;
                break;
            // add other cases if you support arrays / objects …
            default:
                // fallback: store as string
                newVal = valueStr;
        }
    }
    catch (...) {
        // on parse error, we’ll just store the raw string
        newVal = valueStr;
    }

    // Overwrite the stored variable
    it->second.set(std::move(newVal));
}

void Automaton::run() {
    // 0) Broadcast initial snapshot
    broadcastSnapshot();

    while (!m_stop) {
        // 1) Fire immediate (delay==0) transitions
        if (processImmediateTransitions("")) {
            broadcastSnapshot();
        }

        // 2) Compute wait time until next timer
        auto next = scheduler_.nextTimeout();
        auto waitDur = next.value_or(std::chrono::hours(24));

        // 3) Block until timeout, new input, or stop
        std::unique_lock lk(m_mtx);
        m_cv.wait_for(lk, waitDur, [&]{ return m_stop || !m_incoming.empty(); });
        if (m_stop) break;
        lk.unlock();

        auto now = Scheduler::Clock::now();

        // 4) Handle all expired timers
        for (auto idx : scheduler_.popExpired(now)) {
            if (fireTransition(idx, "")) {
                broadcastSnapshot();
            }
        }

        // 5) Handle queued external inputs
        while (true) {
            std::unique_lock lk2(m_mtx);
            if (m_incoming.empty()) break;
            auto [name, val] = std::move(m_incoming.front());
            m_incoming.pop();
            lk2.unlock();

            // update last-seen input and fire transitions
            m_inputs[name] = val;
            if (processImmediateTransitions(name)) {
                broadcastSnapshot();
            }
        }
    }
}