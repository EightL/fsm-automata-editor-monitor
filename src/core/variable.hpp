// variable.hpp
#pragma once

#include <string>
#include <variant>

namespace core_fsm {

/**
 * @brief Variant type for variable values.
 *
 * Supports integer, double, string, and boolean types.
 */
using Value = std::variant<int, double, std::string, bool>;

/**
 * @brief Represents an internal variable of the automaton.
 *
 * Each variable has a name, a type, and a current value.
 */
class Variable {
public:

    /** Supported variable types. */
    enum class Type { Int = 0, Double = 1, String = 2, Bool = 3 };

    /**
     * @brief Construct a new Variable.
     * @param name  Name of the variable.
     * @param type  Type of the variable.
     * @param init  Initial value for the variable.
     */
    Variable(std::string name, Type type, Value init);

    /** @return The variable's name. */
    const std::string& name() const noexcept;

    /** @return The variable's declared type. */
    Type type() const noexcept;

    /** @return The current value. */
    const Value& value() const noexcept;

    /**
     * @brief Assign a new value.
     * @param v  New value (must hold the same type as the variable).
     */
    void set(Value v);

private:
    std::string m_name;
    Type        m_type;
    Value       m_value;
};

} // namespace core_fsm
