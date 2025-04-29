// transition.cpp
#include "transition.hpp"

namespace core_fsm {

Transition::Transition(std::string inputName,
                       GuardFn guard,
                       std::chrono::milliseconds delay,
                       std::size_t src,
                       std::size_t dst)
  : m_inputName(std::move(inputName))
  , m_guard(std::move(guard))
  , m_delay(delay)
  , m_src(src)
  , m_dst(dst)
{}

bool Transition::isTriggered(const std::string& incomingInput,
                             const GuardCtx&    ctx) const
{
    // Must match the input name exactly (both empty = allowable unconditional)
    if (incomingInput != m_inputName) {
        return false;
    }
    // Evaluate guard if provided
    if (m_guard) {
        return m_guard(ctx);
    }
    return true;
}

bool Transition::isDelayed() const noexcept
{
    return m_delay.count() > 0;
}

std::chrono::milliseconds Transition::delay() const noexcept
{
    return m_delay;
}

std::size_t Transition::src() const noexcept
{
    return m_src;
}

std::size_t Transition::dst() const noexcept
{
    return m_dst;
}

} // namespace core_fsm
