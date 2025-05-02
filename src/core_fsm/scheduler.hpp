#pragma once

#include <chrono>
#include <optional>
#include <queue>
#include <vector>

// A simple scheduler for delayed FSM transitions
struct Scheduler {
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Milliseconds = std::chrono::milliseconds;

    // Internal timer record
    struct Timer {
        TimePoint at;
        std::size_t transitionIndex;
    };

    // Compare to make a min-heap (earliest time at top)
    struct Compare {
        bool operator()(Timer const &a, Timer const &b) const {
            return a.at > b.at;
        }
    };

private:
    std::priority_queue<Timer, std::vector<Timer>, Compare> timers_;

public:
    // Arm a transition to fire after 'delay' milliseconds
    void arm(std::size_t transitionIndex, Milliseconds delay) {
        timers_.push(Timer{Clock::now() + delay, transitionIndex});
    }

    // Returns time until the next timer expires, or nullopt if none
    std::optional<Milliseconds> nextTimeout() const {
        if (timers_.empty()) return std::nullopt;
        auto now = Clock::now();
        auto delta = std::chrono::duration_cast<Milliseconds>(timers_.top().at - now);
        // If already expired, return zero
        if (delta.count() < 0) return Milliseconds(0);
        return delta;
    }

    // Pop and return all transition indexes whose timers have expired by 'now'
    std::vector<std::size_t> popExpired(TimePoint now) {
        std::vector<std::size_t> expired;
        while (!timers_.empty() && timers_.top().at <= now) {
            expired.push_back(timers_.top().transitionIndex);
            timers_.pop();
        }
        return expired;
    }

    template<typename Func>
    void purgeForState(size_t activeState, Func getSrc) {
        std::vector<Timer> keep;
        while (!timers_.empty()) {
            auto t = timers_.top(); timers_.pop();
            if (getSrc(t.transitionIndex) == activeState)
                keep.push_back(t);
        }
        for (auto &t : keep) timers_.push(t);
    }
};
