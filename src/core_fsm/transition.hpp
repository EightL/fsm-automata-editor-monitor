// transition.hpp
#pragma once

#include <string>
#include <functional>
#include <chrono>
#include <cstddef>
#include <unordered_map>
#include "variable.hpp"  // for core_fsm::Value

namespace core_fsm {

/**
 * @brief Context passed to guard functions.
 *
 * Contains read‐only access to all variables and the last known inputs.
 */
struct GuardCtx {
    const std::unordered_map<std::string, Value>&         vars;
    const std::unordered_map<std::string, std::string>&   inputs;
};

/**
 * @brief Signature of a guard function.
 * @param ctx  Current guard context.
 * @return     true if the guard condition holds.
 */
using GuardFn = std::function<bool(const GuardCtx&)>;

/**
 * @brief A transition between two states, triggered by an optional input
 *        and/or fired after an optional delay.
 */
class Transition {
public:
    /**
     * @param inputName  Name of the input event that triggers this transition.
     *                   Empty string = no input required.
     * @param guard      Guard function (nullptr = always true).
     * @param delay      Delay before firing (0ms = instantaneous).
     * @param src        Index of source state.
     * @param dst        Index of destination state.
     */
    Transition(std::string      inputName,
               GuardFn          guard,
               std::chrono::milliseconds delay,
               std::size_t      src,
               std::size_t      dst);

    /**
     * @brief Check if this transition is eligible for firing on the given input.
     * @param incomingInput  Name of the input event (empty = none).
     * @param ctx            Guard context.
     * @return true if inputName matches and guard (if any) returns true.
     */
    bool isTriggered(const std::string& incomingInput,
                     const GuardCtx&    ctx) const;

    /** @return true if this transition has a non‐zero delay. */
    bool isDelayed() const noexcept;

    /** @return the transition’s configured delay. */
    std::chrono::milliseconds delay() const noexcept;

    /** @return index of the source state. */
    std::size_t src() const noexcept;

    /** @return index of the destination state. */
    std::size_t dst() const noexcept;

private:
    std::string              m_inputName;
    GuardFn                  m_guard;
    std::chrono::milliseconds m_delay;
    std::size_t              m_src;
    std::size_t              m_dst;
};

} // namespace core_fsm
