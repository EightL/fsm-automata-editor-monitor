/**
 * @file   context.hpp
 * @brief  Defines the Context struct, which wraps references to the
 *         Automaton’s variable, input, and output maps plus timing info,
 *         and provides helper methods for FSM actions.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */

 #pragma once

 #include <unordered_map>
 #include <string>
 #include <variant>
 #include <chrono>
 #include <stdexcept>
 #include "variable.hpp"     ///< for core_fsm::Value
 
 namespace core_fsm {
 
 /**
  * @typedef VarMap
  * @brief  Map from variable names to Variable objects.
  */
 using VarMap = std::unordered_map<std::string, Variable>;
 
 /**
  * @typedef IOMap
  * @brief  Map from input/output names to their last-seen string values.
  */
 using IOMap  = std::unordered_map<std::string, std::string>;
 
 /**
  * @typedef Clock
  * @brief  Steady-clock type for measuring state durations.
  */
 using Clock  = std::chrono::steady_clock;
 
 /**
  * @struct Context
  * @brief  Runtime context passed into state entry and transition guards.
  *
  * Holds references into the Automaton’s internal maps of variables,
  * inputs, and outputs, plus the timestamp when the current state was entered.
  */
 struct Context {
     VarMap&  vars;         ///< Reference to Automaton::m_vars
     IOMap&   inputs;       ///< Reference to Automaton::m_inputs
     IOMap&   outputs;      ///< Reference to Automaton::m_outputs
     Clock::time_point stateSince; ///< Time point when current state was entered
 
     /**
      * @brief Construct a Context binding to the real FSM maps.
      * @param vars_       Reference to the variable map.
      * @param inputs_     Reference to the input map.
      * @param outputs_    Reference to the output map.
      * @param since_      Timestamp of state entry.
      */
     Context(VarMap& vars_,
             IOMap& inputs_,
             IOMap& outputs_,
             Clock::time_point since_)
       : vars(vars_)
       , inputs(inputs_)
       , outputs(outputs_)
       , stateSince(since_)
     {}
 
     /**
      * @brief  Set a variable to a new value.
      * @tparam T   Type of the value (must match declared Variable::Type).
      * @param n   Name of the variable.
      * @param v   New value to assign.
      */
     template<class T>
     void setVar(const std::string& n, const T& v) {
         vars[n].set(Value{v});
     }
 
     /**
      * @brief  Get the current value of a variable.
      * @tparam T   Expected type of the variable’s value.
      * @param n   Name of the variable.
      * @return    The stored value cast to T.
      * @throws    std::runtime_error if the variable is not found.
      */
     template<class T>
     T getVar(const std::string& n) const {
         auto it = vars.find(n);
         if (it == vars.end()) {
             throw std::runtime_error("var not found");
         }
         return std::get<T>(it->second.value());
     }
 
     /**
      * @brief  Check whether an input with the given name is defined.
      * @param in  Input name.
      * @return    True if present in inputs map; false otherwise.
      */
     bool defined(const std::string& in) const noexcept {
         return inputs.find(in) != inputs.end();
     }
 
     /**
      * @brief  Retrieve the last-seen value of an input.
      * @param in  Input name.
      * @return    The input’s string value, or empty string if undefined.
      */
     std::string valueof(const std::string& in) const {
         auto it = inputs.find(in);
         return (it == inputs.end()) ? "" : it->second;
     }
 
     /**
      * @brief  Emit an output value.
      * @param name   Output name.
      * @param val    Value to assign.
      */
     void output(const std::string& name, const std::string& val) {
         outputs[name] = val;
     }
 
     /**
      * @brief  Compute milliseconds elapsed since state entry.
      * @return    Duration in milliseconds since stateSince.
      */
     std::chrono::milliseconds elapsed() const {
         return std::chrono::duration_cast<std::chrono::milliseconds>(
             Clock::now() - stateSince);
     }
 };
 
 }  // namespace core_fsm
 