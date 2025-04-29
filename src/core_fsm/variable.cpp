// variable.cpp
#include "variable.hpp"

using namespace core_fsm;

Variable::Variable(std::string name, Type type, Value init)
  : m_name(std::move(name))
  , m_type(type)
  , m_value(std::move(init))
{}

const std::string& Variable::name() const noexcept {
    return m_name;
}

Variable::Type Variable::type() const noexcept {
    return m_type;
}

const Value& Variable::value() const noexcept {
    return m_value;
}

void Variable::set(Value v) {
    // For simplicity we just overwrite; if you need to enforce
    // that v.index() == static_cast<size_t>(m_type), add a check here.
    m_value = std::move(v);
}
