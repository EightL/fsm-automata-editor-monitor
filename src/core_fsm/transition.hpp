// transition.hpp (updated)
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

struct GuardCtx {
    const std::unordered_map<std::string, Value>& vars;
    const std::unordered_map<std::string, std::string>& inputs;
};

// A transition between two states, with optional input trigger, guard, and delay
class Transition {
public:
    // For fixed‐numeric delays (as before):
    Transition(std::string     inputName,
               std::string     guardExpr,
               std::chrono::milliseconds delay,
               std::size_t     src,
               std::size_t     dst);

    // **NEW** for _variable_ delays:
    Transition(std::string     inputName,
               std::string     guardExpr,
               std::string     delayVarName,  // name of a variable
               std::size_t     src,
               std::size_t     dst);

    bool isTriggered(const std::string& incomingInput,
                     const GuardCtx&    ctx) const;
    bool isDelayed() const noexcept { return m_delay.count() > 0; }
    std::chrono::milliseconds delay() const noexcept { return m_delay; }
    std::size_t src() const noexcept { return m_src; }
    std::size_t dst() const noexcept { return m_dst; }
    
    bool hasVariableDelay() const noexcept { return !m_delayVarName.empty(); }
    const std::string& variableDelayName() const noexcept { return m_delayVarName; }

private:
    std::string              m_inputName;
    std::chrono::milliseconds m_delay{0};
    std::size_t              m_src;
    std::size_t              m_dst;
    
    // when non‐empty, use this var instead of m_delay
    std::string              m_delayVarName;

    // Compiled JS guard function: () -> boolean
    QJSValue                 guardFn_;
    
};

} // namespace core_fsm
