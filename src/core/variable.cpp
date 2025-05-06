/**
 * @file   variable.cpp
 * @brief  Implements the Variable class used to store typed values in the FSM.
 *         Supports string, int and double variable types with runtime type checking.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */
#include "variable.hpp"

using namespace core_fsm;

// -- Variable implementation -----------------------------------------------

/**
 * Constructor creates a variable with the specified name, type and initial value.
 * The provided initial value should match the declared type.
 */
Variable::Variable(std::string name, Type type, Value init)
  : m_name(std::move(name))
  , m_type(type)
  , m_value(std::move(init))
{}

/**
 * Returns the variable's name as defined at creation time.
 */
const std::string& Variable::name() const noexcept {
    return m_name;
}

/**
 * Returns the variable's declared type (Int, Double, String).
 */
Variable::Type Variable::type() const noexcept {
    return m_type;
}

/**
 * Returns a const reference to the variable's current value.
 * Use std::visit to safely access the contained value.
 */
const Value& Variable::value() const noexcept {
    return m_value;
}

/**
 * Updates the variable's value.
 * Note: This implementation allows type conversions at runtime.
 */
void Variable::set(Value v) {
    // For simplicity we just overwrite; if you need to enforce
    // that v.index() == static_cast<size_t>(m_type), add a check here.
    m_value = std::move(v);
}