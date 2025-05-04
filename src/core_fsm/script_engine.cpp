// src/core_fsm/script_engine.cpp
#include "script_engine.hpp"
#include "variable.hpp"        // for core_fsm::Variable::Type

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

    // -- vars: export ints/doubles/bools as *real* JS primitives
    QJSValue varObj = eng.newObject();
    for (const auto& [k, var] : ctx.vars) {
        std::visit([&](auto&& x){
            using T = std::decay_t<decltype(x)>;
            auto name = QString::fromStdString(k);
            if constexpr (std::is_same_v<T, int>)
                varObj.setProperty(name, QJSValue(x));
            else if constexpr (std::is_same_v<T, double>)
                varObj.setProperty(name, QJSValue(x));
            else if constexpr (std::is_same_v<T, bool>)
                varObj.setProperty(name, QJSValue(x));
            else
                varObj.setProperty(name, QJSValue(QString::fromStdString(x)));
        }, var.value());
    }
    obj.setProperty("vars", varObj);

    // -- outputs
    obj.setProperty("outputs", eng.newObject());

    // -- import the C++ "steady_clock" state‐since as a system_clock epoch ms
    {
        using steady = std::chrono::steady_clock;
        using system = std::chrono::system_clock;

        // 1) sample both clocks right now
        auto nowSys    = system::now();
        auto nowSteady = steady::now();

        // 2) compute offset between steady‐epoch and system‐epoch
        auto sysEpoch     = nowSys.time_since_epoch();
        auto steadyEpoch  = nowSteady.time_since_epoch();
        auto offset = sysEpoch 
                    - std::chrono::duration_cast<system::duration>(steadyEpoch);

        // 3) take your saved steady‐clock point and map it into system_clock:
        auto sinceSteadyDur = ctx.stateSince.time_since_epoch();
        auto sinceSysDur = std::chrono::duration_cast<system::duration>(sinceSteadyDur)
                         + offset;

        // 4) export as ms since system epoch
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(sinceSysDur).count();
        obj.setProperty("since", QJSValue(static_cast<double>(ms)));
    }
      
      // install ctx
      eng.globalObject().setProperty("ctx", obj);

    // 3) helper functions — no more stamping in JS
    eng.evaluate(R"js(
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
                    set: function(v)  { ctx.vars[name] = v; }
                });
            }
        })();
    )js");
}

// 5) Pull back any writes to vars & outputs
void pullBack(QJSEngine& eng, Context& ctx) {
    QJSValue obj = eng.globalObject().property("ctx");

    // -- read back vars, converting JS numbers into int/double per declared type
    QJSValue varObj = obj.property("vars");
    QJSValueIterator it(varObj);
    while (it.hasNext()) {
        it.next();
        const std::string name = it.name().toStdString();
        QJSValue            jsVal= it.value();
        auto               iter = ctx.vars.find(name);
        if (iter == ctx.vars.end()) continue;

        auto& var = iter->second;
        switch (var.type()) {
            case Variable::Type::Int:
                // JS numbers may be fractional; cast to int
                var.set(static_cast<int>(jsVal.toNumber()));
                break;
            case Variable::Type::Double:
                var.set(jsVal.toNumber());
                break;
            case Variable::Type::String:
            default:
                var.set(jsVal.toString().toStdString());
                break;
        }
    }

    // -- read back outputs (unchanged)
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
