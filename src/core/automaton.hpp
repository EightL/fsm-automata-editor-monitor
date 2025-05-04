// automaton.hpp
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
 */
class Automaton {
public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration  = std::chrono::milliseconds;

    using SnapshotFn = std::function<void()>;
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

    void setVariable(const std::string& name,
        const std::string& valueStr) noexcept;
        
    /** @brief Add an internal variable (by name). */
    void addVariable(const Variable& var);

    /** @brief Add a state; if initial==true or first state, it becomes the start. */
    void addState(const State& s, bool initial = false);

    /** @brief Add a transition. */
    void addTransition(const Transition& t);

    void broadcastSnapshot();

    bool fireTransition(size_t transitionIndex, const std::string& triggerName);
    /// Runtime API ----------------------------------------------------------

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

    // Attach a transport channel for live I/O
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
    Scheduler scheduler_;
    SnapshotFn m_snapshotHook;

    // For scheduling delayed transitions:
    struct Pending {
        TimePoint   due;
        std::size_t transitionIndex;
        bool operator>(Pending const& o) const { return due > o.due; }
    };

    std::vector<State>           m_states;
    std::vector<Transition>      m_transitions;
    std::size_t                  m_active{0};

    // Last‐known values
    std::unordered_map<std::string, Variable>    m_vars;
    std::unordered_map<std::string, std::string> m_inputs;

    // Timers for delayed transitions
    std::priority_queue<
        Pending,
        std::vector<Pending>,
        std::greater<Pending>
    >                                             m_timers;

    // Input injection & stop signalling
    std::mutex                                    m_mtx;
    std::condition_variable                       m_cv;
    std::queue<std::pair<std::string,std::string>> m_incoming;
    bool                                          m_stop{false};

    // History of entries
    std::vector<EventLog>                         m_log;

    std::unordered_map<std::string,std::string> m_outputs;        // last‐known outputs
    std::chrono::steady_clock::time_point        m_stateSince;    // when we last entered m_active
  
    io_bridge::ChannelPtr   m_channel;
    uint64_t                m_seq{0};
};

} // namespace core_fsm
