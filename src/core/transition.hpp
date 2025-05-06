/** 
 * @file   transition.hpp
 * @brief  Declares the Transition class for FSM transitions,
 *         supporting optional input triggers, JS guards, and fixed or variable delays.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */
#pragma once

#include <string>
#include <chrono>
#include <cstddef>
#include <unordered_map>
#include <QJSEngine>
#include <QJSValue>
#include <QString>
#include "variable.hpp"

namespace core_fsm {

/**
 * @struct GuardCtx
 * @brief Provides current variables and inputs for guard evaluation.
 */
struct GuardCtx {
    const std::unordered_map<std::string, Value>&     vars;    ///< Current variable values
    const std::unordered_map<std::string, std::string>& inputs; ///< Last-seen input values
};

/**
 * @class Transition
 * @brief Represents a transition between two states in the FSM.
 *
 * A Transition may fire when a specified input arrives (or unconditionally),
 * and an optional JavaScript guard evaluates to true.  Supports both fixed
 * numeric delays and dynamic delays based on a variable’s value.
 */
class Transition {
public:
    /**
     * @brief Construct a transition with a fixed delay.
     * @param inputName   Name of input event (empty = unconditional).
     * @param guardExpr   JS boolean expression (empty = no guard).
     * @param delay       Delay before firing (milliseconds).
     * @param src          Index of source state.
     * @param dst          Index of destination state.
     */
    Transition(std::string     inputName,
               std::string     guardExpr,
               std::chrono::milliseconds delay,
               std::size_t     src,
               std::size_t     dst);

    /**
     * @brief Construct a transition with a variable delay.
     * @param inputName     Name of input event (empty = unconditional).
     * @param guardExpr     JS boolean expression (empty = no guard).
     * @param delayVarName  Name of Variable whose int/double value (ms) is used as delay.
     * @param src            Index of source state.
     * @param dst            Index of destination state.
     */
    Transition(std::string     inputName,
               std::string     guardExpr,
               std::string     delayVarName,
               std::size_t     src,
               std::size_t     dst);

    /**
     * @brief Check if this transition should fire on a given input.
     * @param incomingInput  The name of the received input event.
     * @param ctx            GuardCtx providing current vars & inputs.
     * @return True if input matches and JS guard (if any) returns true.
     */
    bool isTriggered(const std::string& incomingInput,
                     const GuardCtx&    ctx) const;

    /** @brief True if there is a fixed (numeric) delay. */
    bool isDelayed() const noexcept { return m_delay.count() > 0; }

    /** @brief Retrieve the fixed numeric delay. */
    std::chrono::milliseconds delay() const noexcept { return m_delay; }

    /** @brief Index of the source state. */
    std::size_t src() const noexcept { return m_src; }

    /** @brief Index of the destination state. */
    std::size_t dst() const noexcept { return m_dst; }

    /** @brief True if using a variable‐based delay. */
    bool hasVariableDelay() const noexcept { return !m_delayVarName.empty(); }

    /** @brief Name of the variable used for dynamic delay. */
    const std::string& variableDelayName() const noexcept { return m_delayVarName; }

private:
    std::string              m_inputName;     ///< Trigger input name
    std::chrono::milliseconds m_delay{0};      ///< Fixed delay in ms
    std::size_t              m_src;           ///< Source state index
    std::size_t              m_dst;           ///< Destination state index
    std::string              m_delayVarName; ///< Variable name for dynamic delay
    QJSValue                 guardFn_;       ///< Compiled JS guard function
};

} // namespace core_fsm
