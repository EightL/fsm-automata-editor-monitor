/**
 * @file   script_engine.cpp
 * @brief  Implements the JavaScript engine utilities to evaluate guards/actions
 *         in the FSM: shared engine management, Context binding, and result pull‐back.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */

 #include "script_engine.hpp"
 #include "variable.hpp"        // for core_fsm::Variable::Type
 
 #include <QJSEngine>
 #include <QJSValue>
 #include <QJSValueIterator>
 #include <chrono>
 
 namespace core_fsm::script {
 
 // -- Shared engine --------------------------------------------------------
 
 /// Return a singleton QJSEngine for all script evaluations.
 QJSEngine& engine() {
     static QJSEngine eng;
     return eng;
 }
 
 // -- Context binding ------------------------------------------------------
 
 /**
  * Bind the C++ FSM Context into the JS global `ctx` object.
  * Exposes inputs, vars, outputs, and the `since` timestamp.
  */
 void bindCtx(QJSEngine& eng, Context& ctx) {
     // Create top‐level ctx object
     QJSValue obj = eng.newObject();
 
     // Populate ctx.inputs
     QJSValue inObj = eng.newObject();
     for (const auto& [k, v] : ctx.inputs) {
         inObj.setProperty(
             QString::fromStdString(k),
             QJSValue(QString::fromStdString(v))
         );
     }
     obj.setProperty("inputs", inObj);
 
     // Populate ctx.vars with correct JS types
     QJSValue varObj = eng.newObject();
     for (const auto& kv : ctx.vars) {
         const auto& key = kv.first;
         const auto& var = kv.second;
         QString qname = QString::fromStdString(key);
 
         std::visit([&varObj, &qname](auto&& x){
             using T = std::decay_t<decltype(x)>;
             if constexpr (std::is_same_v<T, int> ||
                           std::is_same_v<T, double> ||
                           std::is_same_v<T, bool>) {
                 varObj.setProperty(qname, QJSValue(x));
             } else {
                 varObj.setProperty(qname, QJSValue(QString::fromStdString(x)));
             }
         }, var.value());
     }
     obj.setProperty("vars", varObj);
 
     // Create empty ctx.outputs
     obj.setProperty("outputs", eng.newObject());
 
     // Compute and set ctx.since (ms since UNIX epoch)
     {
         using steady = std::chrono::steady_clock;
         using system = std::chrono::system_clock;
         auto nowSys    = system::now();
         auto nowSteady = steady::now();
 
         auto sysEpoch    = nowSys.time_since_epoch();
         auto steadyEpoch = nowSteady.time_since_epoch();
         auto offset = sysEpoch 
                     - std::chrono::duration_cast<system::duration>(steadyEpoch);
 
         auto sinceDur   = ctx.stateSince.time_since_epoch();
         auto sinceSys   = std::chrono::duration_cast<system::duration>(sinceDur) + offset;
         auto ms         = std::chrono::duration_cast<std::chrono::milliseconds>(sinceSys).count();
 
         obj.setProperty("since", QJSValue(static_cast<double>(ms)));
     }
 
     // Install into global scope
     eng.globalObject().setProperty("ctx", obj);
 
     // Add utility JS functions for guard/action scripts
     eng.evaluate(R"js(
         function defined(n) { return n in ctx.inputs || n in ctx.vars; }
         function valueof(n) { return ctx.inputs[n] || ctx.vars[n] || ""; }
         function atoi(s) { return parseInt(s,10) || 0; }
         function elapsed() { return Date.now() - ctx.since; }
         function output(n,v) { ctx.outputs[n] = String(v); }
     )js");
 
     // Alias each variable as a JS global property for convenience
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
 
 // -- Pulling results back -----------------------------------------------
 
 /**
  * Read back any mutations made in JS to ctx.vars and ctx.outputs,
  * updating the original C++ Context accordingly.
  */
 void pullBack(QJSEngine& eng, Context& ctx) {
     QJSValue obj    = eng.globalObject().property("ctx");
 
     // Sync vars: convert JS values to the declared C++ types
     QJSValue varObj = obj.property("vars");
     QJSValueIterator it(varObj);
     while (it.hasNext()) {
         it.next();
         std::string name = it.name().toStdString();
         QJSValue    jsVal = it.value();
         auto        iter  = ctx.vars.find(name);
         if (iter == ctx.vars.end()) continue;
 
         auto& var = iter->second;
         switch (var.type()) {
             case Variable::Type::Int:
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
 
     // Sync outputs: copy all entries from JS back to C++
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
 