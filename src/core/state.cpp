/**
 * @file   state.cpp
 * @brief  Implements core_fsm::State: construction, name lookup, and on-enter action invocation.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */

#include "state.hpp"
#include "context.hpp"

namespace core_fsm {

// Construct a State with a name and optional on-enter action.
State::State(std::string name, ActionFn onEnter)
: m_name(std::move(name))
, m_onEnter(std::move(onEnter))
{}

// Return the state's identifier.
const std::string& State::name() const noexcept {
    return m_name;
}

// Invoke the on-enter action if one was provided.
void State::onEnter(Context& ctx) const {
    if (m_onEnter) {
        m_onEnter(ctx);
    }
}

} // namespace core_fsm
