/**
 * @file   automaton.cpp
 * @brief  Implements Automaton: FSM execution, transition firing, and JSON snapshots.
 *         Provides implementation for the core finite state machine engine including
 *         variable handling, state transitions, guard evaluation, and monitoring.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */

 #include "automaton.hpp"
 #include "scheduler.hpp"
 #include <nlohmann/json.hpp>
 #include <iostream>
 #include <QDebug>
 
 using namespace core_fsm;
 
 namespace {
     // Build a simple name→Value map for guard evaluation
     static std::unordered_map<std::string, Value>
     makeVarSnapshot(const std::unordered_map<std::string, Variable>& vars) {
         std::unordered_map<std::string, Value> snap;
         snap.reserve(vars.size());
         for (auto const& kv : vars)
             snap.emplace(kv.first, kv.second.value());
         return snap;
     }
 }
 
 /**
  * Helper function that converts a Value variant into JSON representation.
  * Uses std::visit to handle different value types appropriately.
  */
 static nlohmann::json jsonFromValue(const core_fsm::Value &v) {
     return std::visit([](auto&& x) -> nlohmann::json { return x; }, v);
 }
 
 /**
  * Registers a new internal variable in the automaton.
  * Stores the variable in the internal map for later use in transitions and scripts.
  */
 void Automaton::addVariable(const Variable& var) {
     // Register a new internal variable
     m_vars.emplace(var.name(), var);
 }
 
 /**
  * Adds a state to the automaton's state collection.
  * If this is the first state or initial=true, it becomes the initial state.
  */
 void Automaton::addState(const State& s, bool initial) {
     // Append state and optionally mark as initial
     m_states.push_back(s);
     if (m_states.size() == 1 || initial)
         m_active = m_states.size() - 1;
 }
 
 /**
  * Adds a transition between states to the automaton.
  * The transition becomes part of the available paths in the state machine.
  */
 void Automaton::addTransition(const Transition& t) {
     // Append transition
     m_transitions.push_back(t);
 }
 
 /**
  * Queues an external input event for processing by the automaton.
  * Thread-safe method that can be called from any context to trigger transitions.
  */
 void Automaton::injectInput(const std::string& name,
                             const std::string& value)
 {
     // Queue external input and wake run loop
     {
         std::lock_guard<std::mutex> lk(m_mtx);
         m_incoming.emplace(name, value);
     }
     m_cv.notify_one();
 }
 
 /**
  * Signals the run loop to terminate execution at the earliest opportunity.
  * Thread-safe method that gracefully requests shutdown of the automaton.
  */
 void Automaton::requestStop() noexcept {
     // Signal run loop to exit
     {
         std::lock_guard<std::mutex> lk(m_mtx);
         m_stop = true;
     }
     m_cv.notify_one();
 }
 
 /**
  * Returns the name of the currently active state.
  * Provides a read-only view of the current state for monitoring purposes.
  */
 const std::string& Automaton::currentState() const noexcept {
     return m_states[m_active].name();
 }
 
 /**
  * Returns the event log containing the history of state transitions.
  * Allows inspection of the execution path for debugging or visualization.
  */
 const std::vector<Automaton::EventLog>&
 Automaton::log() const noexcept
 {
     return m_log;
 }
 
 /**
  * Sends the current state snapshot through the connected channel.
  * Creates a JSON representation of the current state, variables, inputs, and outputs.
  */
 void Automaton::broadcastSnapshot() {
     if (!m_channel) return;
 
     // Build and send JSON snapshot
     nlohmann::json j = {
         {"type",    "state"},
         {"seq",     ++m_seq},
         {"ts",      std::chrono::duration_cast<Duration>(
                         Clock::now().time_since_epoch()).count()},
         {"state",   m_states[m_active].name()},
         {"inputs",  m_inputs},
         {"vars",    [&]{
             nlohmann::json snap;
             for (auto &kv : m_vars)
                 snap[kv.first] = jsonFromValue(kv.second.value());
             return snap;
         }()},
         {"outputs", m_outputs}
     };
     m_channel->send({ j.dump() });
     std::cerr << "RUNTIME → UDP: " << j.dump() << std::endl;
 }
 
 /**
  * Executes a transition between states, updating the active state and triggering side effects.
  * Records the transition in the event log and executes the onEnter action of the target state.
  * 
  * @param idx Index of the transition to fire
  * @param trigger Name of the input that triggered the transition
  * @return true if the transition was successfully executed
  */
 bool Automaton::fireTransition(size_t idx,
                                const std::string& trigger)
 {
     const auto& t = m_transitions[idx];
     if (t.src() != m_active) return false;
 
     // Change state and log event
     auto old = m_active;
     m_active = t.dst();
     m_log.emplace_back(Clock::now(),
                        m_states[m_active].name(),
                        trigger,
                        std::string{});
     if (m_snapshotHook) m_snapshotHook();
 
     // Remove timers for this state and reset timer if changed
     scheduler_.purgeForState(m_active,
         [&](size_t i){ return m_transitions[i].src(); });
     if (m_active != old)
         m_stateSince = Clock::now();
 
     // Invoke onEnter handler
     Context ctx{m_vars, m_inputs, m_outputs, m_stateSince};
     m_states[m_active].onEnter(ctx);
 
     m_inputs.clear();
     return true;
 }
 
 /**
  * Evaluates transitions from the current state and arms those whose guards evaluate to true.
  * Calculates appropriate delays for timed transitions based on fixed or variable delays.
  * 
  * @param trigger Name of the input triggering the evaluation
  * @return true if any immediate transition was fired
  */
 bool Automaton::processImmediateTransitions(const std::string& trigger) {
     // Arm any transitions whose guard fires right now
     auto varSnap = makeVarSnapshot(m_vars);
     GuardCtx guardCtx{varSnap, m_inputs};
 
     for (size_t i = 0; i < m_transitions.size(); ++i) {
         const auto& t = m_transitions[i];
         if (t.src() == m_active &&
             t.isTriggered(trigger, guardCtx))
         {
             // Determine delay: variable, fixed, or 1ms default
             Duration delay{1};
             if (t.hasVariableDelay()) {
                 auto it = m_vars.find(t.variableDelayName());
                 if (it != m_vars.end()) {
                     if (auto iv = std::get_if<int>(&it->second.value()))
                         delay = Duration(*iv);
                     else if (auto dv = std::get_if<double>(&it->second.value()))
                         delay = Duration(static_cast<int>(*dv));
                 }
             }
             else if (t.isDelayed()) {
                 delay = t.delay();
             }
             qDebug() << "[arm]" << t.src() << "→" << t.dst()
                      << "delay=" << delay.count() << "ms";
             scheduler_.arm(i, delay);
         }
     }
     return false;
 }
 
 /**
  * Updates the value of an existing variable from a string representation.
  * Attempts to parse the string according to the variable's type (int, double, string).
  * Falls back to storing as a string if parsing fails.
  */
 void Automaton::setVariable(const std::string& name,
                             const std::string& valueStr) noexcept
 {
     auto it = m_vars.find(name);
     if (it == m_vars.end()) return;
 
     // Parse incoming string into the stored variant type
     try {
         switch (it->second.type()) {
           case Variable::Type::Int:
             it->second.set(std::stoi(valueStr));
             break;
           case Variable::Type::Double:
             it->second.set(std::stod(valueStr));
             break;
           case Variable::Type::String:
           default:
             it->second.set(valueStr);
         }
     }
     catch (...) {
         it->second.set(valueStr);
     }
 }
 
 /**
  * Main execution loop for the automaton that processes events and transitions.
  * Blocks until requestStop() is called, handling inputs and timeouts as they occur.
  * Broadcasts state changes to monitoring clients as they happen.
  */
 void Automaton::run() {
     // Send initial snapshot
     broadcastSnapshot();
 
     while (!m_stop) {
         // Fire zero-delay transitions
         if (processImmediateTransitions(""))
             broadcastSnapshot();
 
         // Wait for the next timeout or input
         auto next = scheduler_.nextTimeout().value_or(std::chrono::hours(24));
         {
             std::unique_lock<std::mutex> lk(m_mtx);
             m_cv.wait_for(lk, next, [&]{ return m_stop || !m_incoming.empty(); });
             if (m_stop) break;
         }
 
         // Handle expired timers
         auto now = Scheduler::Clock::now();
         for (auto idx : scheduler_.popExpired(now)) {
             if (fireTransition(idx, ""))
                 broadcastSnapshot();
         }
 
         // Handle all queued inputs
         while (true) {
             std::pair<std::string,std::string> input;
             {
                 std::unique_lock<std::mutex> lk2(m_mtx);
                 if (m_incoming.empty()) break;
                 input = std::move(m_incoming.front());
                 m_incoming.pop();
             }
             m_inputs[input.first] = input.second;
             if (processImmediateTransitions(input.first))
                 broadcastSnapshot();
         }
     }
 }