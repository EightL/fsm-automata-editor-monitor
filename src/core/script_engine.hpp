/**
 * @file   script_engine.hpp
 * @brief  Embeds a JavaScript engine for guard and action execution
 *         within the FSM, exposing C++ Context to JS.
 *
 * Provides a shared QJSEngine instance, and utilities to bind the
 * core_fsm::Context into the JS environment and pull back modifications.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */

 #pragma once

 #include <QJSEngine>
 #include "context.hpp"
 
 namespace core_fsm::script {
 
 /**
  * @brief Retrieve the shared JavaScript engine instance.
  *
  * Ensures a single QJSEngine is used across all script evaluations
  * to reduce overhead and enable caching of parsed scripts.
  *
  * @return Reference to the shared QJSEngine.
  */
 QJSEngine& engine();
 
 /**
  * @brief Bind the C++ FSM execution Context into the JS engine.
  *
  * Exposes the following objects to the global JS scope:
  *   - ctx.inputs   : map of input names to string values
  *   - ctx.vars     : map of variable names to their current values
  *   - ctx.outputs  : map for emitting output values
  *   - ctx.since    : milliseconds since entering the current state
  *
  * @param eng  The QJSEngine to bind into.
  * @param ctx  The FSM Context containing inputs, vars, outputs, and timestamp.
  */
 void bindCtx(QJSEngine& eng, Context& ctx);
 
 /**
  * @brief Synchronize JS-modified variables and outputs back to C++.
  *
  * After running a script that may have mutated ctx.vars or ctx.outputs
  * in the JS environment, this function reads those properties and
  * updates the original C++ Context accordingly.
  *
  * @param eng  The QJSEngine holding the script-global state.
  * @param ctx  The C++ Context to update with JS-side modifications.
  */
 void pullBack(QJSEngine& eng, Context& ctx);
 
 } // namespace core_fsm::script
 