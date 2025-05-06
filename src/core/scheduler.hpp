/**
 * @file   scheduler.hpp
 * @brief  A simple scheduler for delayed FSM transitions.
 *
 * Provides the Scheduler class, which maintains a min-heap of timers
 * to arm, query, and pop delayed transition events in a finite-state
 * machine.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */

 #pragma once

 #include <chrono>
 #include <optional>
 #include <queue>
 #include <vector>
 
 /**
  * @class Scheduler
  * @brief Manages timers for delayed transitions in a Moore FSM.
  *
  * Scheduler allows arming transitions to fire after a specified delay,
  * querying the next timeout, popping all expired timers, and purging
  * timers when entering a new state.
  */
 struct Scheduler {
     /// Clock type used for scheduling (steady, monotonic).
     using Clock = std::chrono::steady_clock;
     /// Time point in the above clock.
     using TimePoint = Clock::time_point;
     /// Duration in milliseconds.
     using Milliseconds = std::chrono::milliseconds;
 
     /**
      * @struct Timer
      * @brief Internal record of a scheduled transition.
      *
      * Holds the expiration time and the index of the transition to fire.
      */
     struct Timer {
         TimePoint   at;               ///< When the timer expires
         std::size_t transitionIndex;  ///< Corresponding transition index
     };
 
     /**
      * @struct Compare
      * @brief Comparator to turn std::priority_queue into a min-heap.
      *
      * Returns true if a's expiration is later than b's, so the earliest
      * Timer is at the top of the heap.
      */
     struct Compare {
         bool operator()(Timer const &a, Timer const &b) const {
             return a.at > b.at;
         }
     };
 
 private:
     /// Min-heap of pending timers (earliest expiration at top).
     std::priority_queue<Timer, std::vector<Timer>, Compare> timers_;
 
 public:
     /**
      * @brief Arm a transition to fire after a given delay.
      *
      * @param transitionIndex  Index of the transition to schedule.
      * @param delay            Delay from now until firing, in ms.
      */
     void arm(std::size_t transitionIndex, Milliseconds delay) {
         timers_.push(Timer{Clock::now() + delay, transitionIndex});
     }
 
     /**
      * @brief Time until the next timer expires.
      *
      * @return Optional delay until the earliest timer; std::nullopt if
      *         no timers are pending.  If already expired, returns zero.
      */
     std::optional<Milliseconds> nextTimeout() const {
         if (timers_.empty()) return std::nullopt;
         auto now = Clock::now();
         auto delta = std::chrono::duration_cast<Milliseconds>(timers_.top().at - now);
         if (delta.count() < 0) return Milliseconds(0);
         return delta;
     }
 
     /**
      * @brief Pop and retrieve all transition indices whose timers
      *        have expired by the given time.
      *
      * @param now  The current time point against which to compare.
      * @return     Vector of transition indices whose timers expired.
      */
     std::vector<std::size_t> popExpired(TimePoint now) {
         std::vector<std::size_t> expired;
         while (!timers_.empty() && timers_.top().at <= now) {
             expired.push_back(timers_.top().transitionIndex);
             timers_.pop();
         }
         return expired;
     }
 
     /**
      * @brief Remove timers not originating from the active state.
      *
      * When entering a new state, any timers whose source state differs
      * from `activeState` are purged.  This avoids firing stale transitions.
      *
      * @tparam Func  Callable receiving a transitionIndex and returning
      *               its source-state index.
      * @param activeState  The index of the newly active state.
      * @param getSrc       Function mapping transitionIndex → source state.
      */
     template<typename Func>
     void purgeForState(size_t activeState, Func getSrc) {
         std::vector<Timer> keep;
         while (!timers_.empty()) {
             auto t = timers_.top();
             timers_.pop();
             if (getSrc(t.transitionIndex) == activeState) {
                 keep.push_back(t);
             }
         }
         for (auto &t : keep) {
             timers_.push(t);
         }
     }
 };
 