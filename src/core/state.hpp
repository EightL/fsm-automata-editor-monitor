/**
 * @file   state.hpp
 * @brief  Declares core_fsm::State, representing a named FSM state with an optional entry action.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný   (xlucnyj00)
 * @date   2025-05-06
 */

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
  * @param ctx  Current execution context (variables, inputs, outputs, timing).
  */
 using ActionFn = std::function<void(Context&)>;
 
 /**
  * @class State
  * @brief Represents a state in the finite-state machine.
  *
  * Each State has a unique identifier and an optional on-enter action
  * which is invoked when the FSM transitions into this state.
  */
 class State {
 public:
     /**
      * @brief Construct a new State.
      *
      * @param name     The unique identifier of the state.
      * @param onEnter  Optional callback to execute upon state entry.
      */
     State(std::string name, ActionFn onEnter = {});
 
     /**
      * @brief Get the state's identifier.
      * @return Reference to the state's name string.
      */
     const std::string& name() const noexcept;
 
     /**
      * @brief Invoke the on-enter action, if provided.
      *
      * @param ctx  Current context exposing variables, inputs, outputs, and elapsed time.
      */
     void onEnter(Context& ctx) const;
 
 private:
     std::string m_name;    ///< Unique state name
     ActionFn    m_onEnter; ///< Entry action callback (may be empty)
 };
 
 } // namespace core_fsm
 