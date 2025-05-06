/**
 * @file   variable.hpp
 * @brief  Defines the Variable class for storing typed values in the FSM.
 *         Provides strong typing with runtime type checking and value storage.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */
#pragma once

#include <string>
#include <variant>

namespace core_fsm {

/**
 * @brief Variant type for variable values.
 *
 * Supports integer, double, string, and boolean types which can be
 * safely accessed using std::visit patterns for type-safe operations.
 */
using Value = std::variant<int, double, std::string, bool>;

/**
 * @brief Represents an internal variable of the automaton.
 *
 * Each variable has a name, a type, and a current value. Variables are used
 * to store state information that persists across state transitions and can
 * be accessed from JavaScript guards and actions via the context object.
 */
class Variable {
public:

    /**
     * @brief Supported variable types.
     * 
     * The integer values correspond to the index in the Value variant.
     */
    enum class Type { Int = 0, Double = 1, String = 2, Bool = 3 };

    /**
     * @brief Construct a new Variable.
     * @param name  Name of the variable.
     * @param type  Type of the variable.
     * @param init  Initial value for the variable.
     */
    Variable(std::string name, Type type, Value init);

    /** 
     * @return The variable's name.
     */
    const std::string& name() const noexcept;

    /** 
     * @return The variable's declared type (Int, Double, String, Bool).
     */
    Type type() const noexcept;

    /** 
     * @return The current value as a std::variant.
     */
    const Value& value() const noexcept;

    /**
     * @brief Assign a new value.
     * @param v  New value to store in this variable.
     * 
     * Note: This implementation allows type conversions at runtime.
     * No type checking is performed against the declared variable type.
     */
    void set(Value v);

private:
    std::string m_name;  ///< Variable name
    Type        m_type;  ///< Declared type
    Value       m_value; ///< Current value
};

} // namespace core_fsm
