/**
 * @file   automaton.hpp
 * @brief  Core finite state machine implementation for the FSM editor project.
 *         Defines the Automaton class that powers the runtime state machine engine,
 *         supporting states, transitions, guards, delays, variables, and I/O operations.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */
#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "scheduler.hpp"    // at the top

#include "variable.hpp"
#include "transition.hpp"
#include "state.hpp"
#include "io/channel.hpp" 

namespace core_fsm {

/**
 * @brief Drives a Moore-style timed finite-state machine.
 * 
 * Provides a complete implementation of a finite state machine with support for:
 * - Multiple states with entry actions
 * - Transitions with triggers, guards, and delays
 * - Internal variables with JavaScript evaluation
 * - Input/output management
 * - Event logging for monitoring
 * - Thread-safe execution
 */
class Automaton {
public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration  = std::chrono::milliseconds;

    using SnapshotFn = std::function<void()>;
    
    /**
     * @brief Sets a callback function to be called when state changes occur
     * @param cb Function to be called for state snapshots
     */
    void   setSnapshotHook(SnapshotFn cb) { m_snapshotHook = std::move(cb); }

    /** @brief A record of a state entry (for logging/monitoring). */
    struct EventLog {
        TimePoint   timestamp;
        std::string state;
        std::string triggerInput;  // empty for timeouts
        std::string triggerValue;

        // Constructor for emplace_back compatibility
        EventLog(TimePoint timestamp_,
                 const std::string& state_,
                 const std::string& triggerInput_,
                 const std::string& triggerValue_)
          : timestamp(timestamp_), state(state_), triggerInput(triggerInput_), triggerValue(triggerValue_)
        {}
    };

    Automaton() = default;
    ~Automaton() = default;

    Automaton(const Automaton&) = delete;
    Automaton& operator=(const Automaton&) = delete;

    /// Build the model ------------------------------------------------------

    /**
     * @brief Updates the value of an existing variable
     * @param name The name of the variable to update
     * @param valueStr String representation of the new value
     */
    void setVariable(const std::string& name,
        const std::string& valueStr) noexcept;
        
    /** @brief Add an internal variable (by name). */
    void addVariable(const Variable& var);

    /** @brief Add a state; if initial==true or first state, it becomes the start. */
    void addState(const State& s, bool initial = false);

    /** @brief Add a transition. */
    void addTransition(const Transition& t);

    /**
     * @brief Sends current state snapshot to connected channels
     * 
     * Broadcasts the current state, variables, inputs, and outputs through
     * the connected communication channel, if available.
     */
    void broadcastSnapshot();

    /**
     * @brief Executes the specified transition
     * @param transitionIndex Index of the transition to execute
     * @param triggerName Name of the input that triggered the transition
     * @return true if transition was successful, false otherwise
     */
    bool fireTransition(size_t transitionIndex, const std::string& triggerName);

    /// Runtime API ----------------------------------------------------------

    /**
     * @brief Evaluates and executes any transitions that can fire immediately
     * @param trigger Name of the input that might trigger transitions
     * @return true if any transition was fired, false otherwise
     */
    bool processImmediateTransitions(const std::string& trigger);

    /** @brief Called by external code/threads to inject an input event. */
    void injectInput(const std::string& name,
                     const std::string& value);

    /** @brief Ask the `run()` loop to exit at the next opportunity. */
    void requestStop() noexcept;

    /** @brief Blocking interpreter loop; returns when `requestStop()` is called. */
    void run();

    /// Inspection -----------------------------------------------------------

    /** @return The name of the current active state. */
    const std::string& currentState() const noexcept;

    /** @return All state‐entry events recorded so far. */
    const std::vector<EventLog>& log() const noexcept;

    /**
     * @brief Connects an I/O channel for runtime communication
     * @param ch The channel to attach for bidirectional communication
     */
    void attachChannel(io_bridge::ChannelPtr ch) {
        m_channel = std::move(ch);
    }

public:
    /// Current registered inputs (name → last‐seen value)
    const std::unordered_map<std::string, std::string>& inputs() const noexcept {
        return m_inputs;
    }

    /// Current variables (name → Variable)
    const std::unordered_map<std::string, Variable>& vars() const noexcept {
        return m_vars;
    }

    /// Current outputs (name → last‐emitted value)
    const std::unordered_map<std::string, std::string>& outputs() const noexcept {
        return m_outputs;
    }
    
    
private:
    Scheduler scheduler_;           // Scheduler for time-based transitions
    SnapshotFn m_snapshotHook;      // Callback for state changes

    // For scheduling delayed transitions:
    struct Pending {
        TimePoint   due;            // When the transition should fire
        std::size_t transitionIndex;// Index into m_transitions
        bool operator>(Pending const& o) const { return due > o.due; }
    };

    std::vector<State>           m_states;       // All defined states
    std::vector<Transition>      m_transitions;  // All defined transitions
    std::size_t                  m_active{0};    // Index of current active state

    // Last‐known values
    std::unordered_map<std::string, Variable>    m_vars;    // Variables and their values
    std::unordered_map<std::string, std::string> m_inputs;  // Input values

    // Timers for delayed transitions
    std::priority_queue<
        Pending,
        std::vector<Pending>,
        std::greater<Pending>
    >                                             m_timers;  // Ordered by due time

    // Input injection & stop signalling
    std::mutex                                    m_mtx;     // Protects the queue
    std::condition_variable                       m_cv;      // For run loop wakeup
    std::queue<std::pair<std::string,std::string>> m_incoming;// Input queue
    bool                                          m_stop{false}; // Stop flag

    // History of entries
    std::vector<EventLog>                         m_log;     // State entry log

    std::unordered_map<std::string,std::string> m_outputs;    // last‐known outputs
    std::chrono::steady_clock::time_point        m_stateSince; // when we last entered m_active
  
    io_bridge::ChannelPtr   m_channel;           // Communication channel
    uint64_t                m_seq{0};            // Sequence counter for messages
};

} // namespace core_fsm