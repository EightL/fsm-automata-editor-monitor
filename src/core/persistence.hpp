/**
 * @file   persistence.hpp
 * @brief  Defines the in-memory FSM document model and JSON persistence API.
 *
 * This header declares the FsmDocument struct (with VariableDesc,
 * StateDesc, TransitionDesc) for representing `.fsm` files in memory,
 * and the loadFile()/saveFile() functions to read/write FSMs as JSON
 * with basic schema and semantic checks.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */

 #ifndef CORE_FSM_PERSISTENCE_HPP
 #define CORE_FSM_PERSISTENCE_HPP
 
 #include <string>
 #include <vector>
 #include <chrono>
 #include <nlohmann/json.hpp>
 using ordered_json = nlohmann::ordered_json;
 
 #include "automaton.hpp"
 #include "state.hpp"
 #include "transition.hpp"
 #include "variable.hpp"
 
 namespace core_fsm::persistence {
 
 /**
  * @struct VariableDesc
  * @brief Description of an internal variable in the FSM.
  *
  * Holds the name, type (e.g. "int", "double", "string"), and
  * the raw JSON literal for the initial value.
  */
 struct VariableDesc {
     std::string      name;   ///< Variable name
     std::string      type;   ///< Type name (e.g. "int", "float", "string")
     nlohmann::json   init;   ///< Initial value (preserved as JSON)
 };
 
 /**
  * @struct StateDesc
  * @brief Description of a state in the FSM.
  *
  * Contains the state identifier, a flag indicating whether it is
  * the initial state, and optional code to execute on entry.
  */
 struct StateDesc {
     std::string id;           ///< State identifier
     bool        initial = false;  ///< true if this is the start state
     std::string onEnter;      ///< Raw code snippet to run on entry
 };
 
 /**
  * @struct TransitionDesc
  * @brief Description of a transition in the FSM.
  *
  * Specifies the source and destination state IDs, optional
  * trigger event name, guard expression, and a delay (either
  * a numeric literal or a variable name) stored as JSON.
  */
 struct TransitionDesc {
     std::string    from;       ///< Source state ID
     std::string    to;         ///< Destination state ID
     std::string    trigger;    ///< Input event name ("" = unconditional)
     std::string    guard;      ///< Guard expression ("" = always true)
     nlohmann::json delay_ms;   ///< Delay in milliseconds (int or var name)
 };
 
 /**
  * @struct FsmDocument
  * @brief In-memory representation of a `.fsm` document.
  *
  * Aggregates the automaton name, comment, inputs, outputs,
  * variables, states, and transitions as parsed from JSON.
  */
 struct FsmDocument {
     std::string                name;         ///< Automaton name
     std::string                comment;      ///< Optional comment
     std::vector<std::string>   inputs;       ///< Declared input names
     std::vector<std::string>   outputs;      ///< Declared output names
     std::vector<VariableDesc>  variables;    ///< Internal variables
     std::vector<StateDesc>     states;       ///< State descriptions
     std::vector<TransitionDesc> transitions;  ///< Transition descriptions
 };
 
 /**
  * @brief Load an FSM from a JSON file on disk.
  *
  * Opens the file at `path`, parses its JSON contents into an ordered JSON,
  * performs basic sanity checks (unknown triggers, guard references),
  * and converts it into an FsmDocument.
  *
  * @param path File path to read from.
  * @param out  Document to populate upon success.
  * @param err  Optional out-param that receives an error or warning
  *             message on failure or non-fatal warning.
  * @return     true if the file was successfully read (even with warnings),
  *             false on fatal I/O or parse errors.
  */
 bool loadFile(const std::string& path,
               FsmDocument& out,
               std::string* err = nullptr);
 
 /**
  * @brief Save an FSM document to disk as JSON.
  *
  * Serializes `doc` into JSON and writes it to `path`.  If `pretty`
  * is true, outputs with 4-space indentation.
  *
  * @param doc    Document to serialize.
  * @param path   Output file path.
  * @param pretty Whether to pretty-print with indentation.
  * @param err    Optional out-param for error messages on failure.
  * @return       true on success; false if the file could not be opened
  *               or serialization failed.
  */
 bool saveFile(const FsmDocument& doc,
               const std::string& path,
               bool pretty = true,
               std::string* err = nullptr);
 
 } // namespace core_fsm::persistence
 
 // ----------------------------------------------------------------------------
 // nlohmann::json ADL serializers for persistence types
 // ----------------------------------------------------------------------------
 namespace nlohmann {
 
 template <>
 struct adl_serializer<core_fsm::persistence::VariableDesc> {
     static void to_json(ordered_json& j, core_fsm::persistence::VariableDesc const& v) {
         j = ordered_json{
             {"name", v.name},
             {"type", v.type},
             {"init", v.init}
         };
     }
     static void from_json(ordered_json const& j, core_fsm::persistence::VariableDesc& v) {
         j.at("name").get_to(v.name);
         j.at("type").get_to(v.type);
         j.at("init").get_to(v.init);
     }
 };
 
 template <>
 struct adl_serializer<core_fsm::persistence::StateDesc> {
     static void to_json(ordered_json& j, core_fsm::persistence::StateDesc const& s) {
         j = ordered_json{{"id", s.id}};
         if (s.initial)    j["initial"] = true;
         if (!s.onEnter.empty()) j["onEnter"] = s.onEnter;
     }
     static void from_json(ordered_json const& j, core_fsm::persistence::StateDesc& s) {
         j.at("id").get_to(s.id);
         if (j.contains("initial"))  j.at("initial").get_to(s.initial);
         if (j.contains("onEnter"))  j.at("onEnter").get_to(s.onEnter);
     }
 };
 
 template <>
 struct adl_serializer<core_fsm::persistence::TransitionDesc> {
     static void to_json(ordered_json& j, core_fsm::persistence::TransitionDesc const& t) {
         j = ordered_json{
             {"from", t.from},
             {"to",   t.to}
         };
         if (!t.trigger.empty())    j["trigger"]  = t.trigger;
         if (!t.guard.empty())      j["guard"]    = t.guard;
         if (!t.delay_ms.is_null()) j["delay_ms"] = t.delay_ms;
     }
     static void from_json(ordered_json const& j, core_fsm::persistence::TransitionDesc& t) {
         j.at("from").get_to(t.from);
         j.at("to").get_to(t.to);
         if (j.contains("trigger"))  j.at("trigger").get_to(t.trigger);
         if (j.contains("guard"))    j.at("guard").get_to(t.guard);
         if (j.contains("delay_ms")) j.at("delay_ms").get_to(t.delay_ms);
     }
 };
 
 template <>
 struct adl_serializer<core_fsm::persistence::FsmDocument> {
     static void to_json(ordered_json& j, core_fsm::persistence::FsmDocument const& d) {
         j = ordered_json{
             {"name",        d.name},
             {"comment",     d.comment},
             {"inputs",      d.inputs},
             {"outputs",     d.outputs},
             {"variables",   d.variables},
             {"states",      d.states},
             {"transitions", d.transitions}
         };
     }
     static void from_json(ordered_json const& j, core_fsm::persistence::FsmDocument& d) {
         if (j.contains("id"))   j.at("id").get_to(d.name);
         else                     j.at("name").get_to(d.name);
         if (j.contains("comment"))   j.at("comment").get_to(d.comment);
         j.at("inputs")      .get_to(d.inputs);
         j.at("outputs")     .get_to(d.outputs);
         j.at("variables")   .get_to(d.variables);
         j.at("states")      .get_to(d.states);
         j.at("transitions") .get_to(d.transitions);
     }
 };
 
 } // namespace nlohmann
 
 #endif // CORE_FSM_PERSISTENCE_HPP
 