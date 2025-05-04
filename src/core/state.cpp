// state.cpp
#include "state.hpp"
#include "context.hpp"

namespace core_fsm {

State::State(std::string name, ActionFn onEnter)
  : m_name(std::move(name))
  , m_onEnter(std::move(onEnter))
{}

const std::string& State::name() const noexcept {
    return m_name;
}

void State::onEnter(Context& ctx) const {
    if (m_onEnter) {
        m_onEnter(ctx);
    }
}

} // namespace core_fsm
