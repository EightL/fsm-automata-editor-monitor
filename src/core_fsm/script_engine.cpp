// src/core_fsm/script_engine.cpp
#include "script_engine.hpp"

#include <QJSEngine>
#include <QJSValue>
#include <QJSValueIterator>
#include <chrono>

namespace core_fsm::script {

// 1) Shared engine
QJSEngine& engine() {
    static QJSEngine eng;
    return eng;
}

// 2) Bind C++ Context into the JS global 'ctx'
void bindCtx(QJSEngine& eng, Context& ctx) {
    // Create ctx = { inputs:{}, vars:{}, outputs:{}, since:<ms> }
    QJSValue obj = eng.newObject();

    // -- inputs
    QJSValue inObj = eng.newObject();
    for (const auto& [k, v] : ctx.inputs) {
        inObj.setProperty(
            QString::fromStdString(k),
            QJSValue(QString::fromStdString(v))
        );
    }
    obj.setProperty("inputs", inObj);

    // -- vars
    QJSValue varObj = eng.newObject();
    for (const auto& [k, var] : ctx.vars) {
        QString s = std::visit([](auto&& x) -> QString {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, bool>)    return x ? "1" : "0";
            else if constexpr (std::is_same_v<T, int>)      return QString::number(x);
            else if constexpr (std::is_same_v<T, double>)   return QString::number(x);
            else                                            return QString::fromStdString(x);
        }, var.value());
        varObj.setProperty(
            QString::fromStdString(k),
            QJSValue(s)
        );
    }
    obj.setProperty("vars", varObj);

    // -- outputs
    obj.setProperty("outputs", eng.newObject());

    // install ctx into the JS global scope
    eng.globalObject().setProperty("ctx", obj);

    // 3) helper functions + JS‐side since stamp
    eng.evaluate(R"js(
        // stamp the entry time
        ctx.since = Date.now();

        function defined(n) { return n in ctx.inputs || n in ctx.vars; }
        function valueof(n) { return ctx.inputs[n] || ctx.vars[n] || ""; }
        function atoi(s) { return parseInt(s,10) || 0; }
        function elapsed() { return Date.now() - ctx.since; }
        function output(n,v) { ctx.outputs[n] = String(v); }
    )js");

    // 4) alias every variable as a real global property
    eng.evaluate(R"js(
        (function(){
            for (let name in ctx.vars) {
                Object.defineProperty(this, name, {
                    get: function() { return ctx.vars[name]; },
                    set: function(v)  { ctx.vars[name] = String(v); }
                });
            }
        })();
    )js");
}

// 5) Pull back any writes to vars & outputs
void pullBack(QJSEngine& eng, Context& ctx) {
    QJSValue obj = eng.globalObject().property("ctx");

    // -- read back vars
    QJSValue varObj = obj.property("vars");
    QJSValueIterator it(varObj);
    while (it.hasNext()) {
        it.next();
        std::string name = it.name().toStdString();
        std::string val  = it.value().toString().toStdString();
        auto   iter     = ctx.vars.find(name);
        if (iter != ctx.vars.end()) {
            iter->second.set(val);
        }
    }

    // -- read back outputs
    QJSValue outObj = obj.property("outputs");
    QJSValueIterator it2(outObj);
    while (it2.hasNext()) {
        it2.next();
        std::string name = it2.name().toStdString();
        std::string val  = it2.value().toString().toStdString();
        ctx.outputs[name] = val;
    }
}

} // namespace core_fsm::script
