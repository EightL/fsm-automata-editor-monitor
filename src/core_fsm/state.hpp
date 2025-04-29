// state.hpp
#pragma once

#include <string>
#include <functional>
#include "transition.hpp"
#include "context.hpp"

namespace core_fsm {

/**
 * @brief Signature of a state entry action.
 *
 * Executed whenever the automaton enters this state.
 * @param ctx  Current guard context (variables & inputs).
 */
using ActionFn = std::function<void(Context&)>;

/**
 * @brief Represents a state in the finite-state machine.
 *
 * Each state has a unique name and an optional on-enter action.
 */
class State {
public:
    /**
     * @brief Construct a new State.
     * @param name     The state’s identifier.
     * @param onEnter  Action to run on entry (empty = no action).
     */
    State(std::string name, ActionFn onEnter = {});

    /** @return The state’s name. */
    const std::string& name() const noexcept;

    /**
     * @brief Invoke the on-enter action, if one was provided.
     * @param ctx  Current context (variables & last inputs).
     */
    void onEnter(Context& ctx) const;

private:
    std::string m_name;
    ActionFn    m_onEnter;
};

} // namespace core_fsm
