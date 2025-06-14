/**
 * @file   transition.cpp
 * @brief  Implements the Transition class for FSM state transitions,
 *         with support for input triggers, JS guards, and delay mechanisms.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */
#include "transition.hpp"
#include <stdexcept>
#include <variant>

namespace core_fsm {

    namespace {
    // -- JS Engine Singleton ------------------------------------------------
    
    /**
     * Returns a singleton QJSEngine instance with predefined helper functions.
     * Sets up JS utility functions that are available in all guard expressions:
     * - valueof(name): Gets value from inputs or vars (inputs take precedence)
     * - defined(name): Checks if name exists in inputs or vars
     * - atoi(s): Converts string to integer (parseInt wrapper)
     */
    QJSEngine& engine() {
        static bool initialized = false;
        static QJSEngine e;
        if (!initialized) {
            /*  Helper functions that will be visible from every guard.
                -------------------------------------------------------
                • valueof(name)  – returns **last known value** of an input
                                  OR of a variable (inputs shadow variables).
                • defined(name)  – true when the symbol is present either in
                                  inputs **or** variables.
                • atoi(s)        – convenience wrapper around parseInt.
            */
            e.evaluate(R"(
                function valueof(name)
                {
                    if (Object.prototype.hasOwnProperty.call(ctx.inputs, name))
                        return String(ctx.inputs[name]);   // any JS primitive → str

                    if (Object.prototype.hasOwnProperty.call(ctx.vars, name))
                        return String(ctx.vars[name]);

                    return "";                             // unknown → empty str
                }

                function defined(name)
                {
                    return Object.prototype.hasOwnProperty.call(ctx.inputs, name) ||
                           Object.prototype.hasOwnProperty.call(ctx.vars,   name);
                }

                function atoi(s) { return parseInt(s, 10); }
            )");
            initialized = true;
        }
        return e;
    }
}

// -- Transition implementations -------------------------------------------

/**
 * Construct a transition with a fixed numeric delay.
 * Compiles the guard expression into a callable JS function if provided.
 */
Transition::Transition(std::string inputName,
                       std::string guardExpr,
                       std::chrono::milliseconds delay,
                       std::size_t src,
                       std::size_t dst)
  : m_inputName(std::move(inputName))
  , m_delay(delay)
  , m_src(src)
  , m_dst(dst)
{
    if (!guardExpr.empty()) {
        // Wrap the expression in a JS function () => <expr>
        QString jsFn = QString("(function(){ return %1; })").arg(
            QString::fromStdString(guardExpr)
        );
        QJSValue fn = engine().evaluate(jsFn);
        if (!fn.isCallable()) {
            throw std::runtime_error("Guard compile error: " + guardExpr);
        }
        guardFn_ = fn;
    }
}

/**
 * Construct a transition with a variable-based delay.
 * Compiles the guard expression into a callable JS function if provided.
 */
Transition::Transition(std::string inputName,
                       std::string guardExpr,
                       std::string delayVarName,
                       std::size_t src,
                       std::size_t dst)
  : m_inputName(std::move(inputName))
  , m_delayVarName(std::move(delayVarName))
  , m_src(src)
  , m_dst(dst)
{
    if (!guardExpr.empty()) {
        QString jsFn = QString("(function(){ return %1; })")
                        .arg(QString::fromStdString(guardExpr));
        QJSValue fn = engine().evaluate(jsFn);
        if (!fn.isCallable())
            throw std::runtime_error("Guard compile error: " + guardExpr);
        guardFn_ = fn;
    }
}

/**
 * Determines if this transition should fire based on input and guard condition.
 * First checks if input name matches, then evaluates the guard expression
 * with the current context if a guard exists.
 */
bool Transition::isTriggered(const std::string& incomingInput,
                             const GuardCtx&    ctx) const
{
    // Input must match (empty==unconditional)
    if (incomingInput != m_inputName) return false;

    // No guard => always true
    if (!guardFn_.isCallable()) return true;

    // Build a JS context object { inputs: {...}, vars: {...} }
    QJSEngine& eng = engine();
    QJSValue jsCtx = eng.newObject();

    // Populate ctx.inputs with string values
    QJSValue jsInputs = eng.newObject();
    for (auto const& kv : ctx.inputs) {
        jsInputs.setProperty(
            QString::fromStdString(kv.first),
            QString::fromStdString(kv.second)
        );
    }
    jsCtx.setProperty("inputs", jsInputs);

    // Populate ctx.vars with type-appropriate values
    QJSValue jsVars = eng.newObject();
    for (auto const& kv : ctx.vars) {
        QJSValue v;
        std::visit([&](auto&& x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, int>)
                v = QJSValue(x);
            else if constexpr (std::is_same_v<T, double>)
                v = QJSValue(x);
            else if constexpr (std::is_same_v<T, std::string>)
                v = QJSValue(QString::fromStdString(x));
        }, kv.second);
        jsVars.setProperty(QString::fromStdString(kv.first), v);
    }
    jsCtx.setProperty("vars", jsVars);

    // Make context accessible to helper functions
    eng.globalObject().setProperty("ctx", jsCtx);

    // Evaluate guard function and handle errors
    QJSValue fn = guardFn_;
    QJSValue result = fn.call();
    if (result.isError()) {
        throw std::runtime_error(
            "JS guard error: " + result.toString().toStdString());
    }
    return result.toBool();
}

} // namespace core_fsm
