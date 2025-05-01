#ifndef CORE_FSM_PERSISTENCE_HPP
#define CORE_FSM_PERSISTENCE_HPP

#include <string>
#include <vector>
#include <chrono>
#include "../../external/nlohmann/json.hpp"
using ordered_json = nlohmann::ordered_json;


#include "automaton.hpp"
#include "state.hpp"
#include "transition.hpp"
#include "variable.hpp"


namespace core_fsm::persistence {

// std::chrono::milliseconds
// parseDelay(const nlohmann::json& j, const core_fsm::VarMap& vars);


/**
 * @brief Description of an internal variable in the FSM.
 */
struct VariableDesc {
    std::string      name;   ///< Variable name
    std::string      type;   ///< Type name (e.g. "int", "float", "string")
    nlohmann::json   init;   ///< Initial value (kept as JSON to preserve literal)
};

/**
 * @brief Description of a state in the FSM.
 */
struct StateDesc {
    std::string id;        ///< State identifier
    bool        initial = false;  ///< True if this is the start state
    std::string onEnter;   ///< Raw code to execute on entry
};

/**
 * @brief Description of a transition in the FSM.
 */
struct TransitionDesc {
    std::string    from;       ///< Source state ID
    std::string    to;         ///< Destination state ID
    std::string    trigger;    ///< Input event name ("" = unconditional)
    std::string    guard;      ///< Guard expression ("" = always true)
    nlohmann::json delay_ms;   ///< Delay in ms, integer or string variable name
};

/**
 * @brief In-memory representation of a .fsm document.
 */
struct FsmDocument {
    std::string               name;         ///< Automaton name
    std::string               comment;      ///< Optional comment
    std::vector<std::string>  inputs;       ///< List of input names
    std::vector<std::string>  outputs;      ///< List of output names
    std::vector<VariableDesc> variables;    ///< Internal variables
    std::vector<StateDesc>    states;       ///< States (first with initial=true is start)
    std::vector<TransitionDesc> transitions; ///< All transitions
};

/**
 * @brief Load an FSM from a JSON file on disk.
 * @param path File path
 * @param out  Document to populate
 * @param err  Optional error message on failure
 * @return true on success, false otherwise
 */
bool loadFile(const std::string& path,
              FsmDocument& out,
              std::string* err = nullptr);

/**
 * @brief Save an FSM document to disk as JSON.
 * @param doc    Document to serialize
 * @param path   Output file path
 * @param pretty If true, writes with indentation
 * @param err    Optional error message on failure
 * @return true on success, false otherwise
 */
bool saveFile(const FsmDocument& doc,
              const std::string& path,
              bool pretty = true,
              std::string* err = nullptr);

} // namespace core_fsm::persistence

// nlohmann::json serializers/deserializers
namespace nlohmann {

template <>
struct adl_serializer<core_fsm::persistence::VariableDesc> {
    static void to_json(ordered_json& j, core_fsm::persistence::VariableDesc const& v) {
        j.clear();
        j["name"] = v.name;
        j["type"] = v.type;
        j["init"] = v.init;
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
        j.clear();
        j["id"] = s.id;
        if (s.initial) j["initial"] = true;
        if (!s.onEnter.empty()) j["onEnter"] = s.onEnter;
    }
    static void from_json(ordered_json const& j, core_fsm::persistence::StateDesc& s) {
        j.at("id").get_to(s.id);
        if (j.contains("initial")) j.at("initial").get_to(s.initial);
        if (j.contains("onEnter")) j.at("onEnter").get_to(s.onEnter);
    }
};

template <>
struct adl_serializer<core_fsm::persistence::TransitionDesc> {
    static void to_json(ordered_json& j, core_fsm::persistence::TransitionDesc const& t) {
        j.clear();
        j["from"] = t.from;
        j["to"] = t.to;
        if (!t.trigger.empty()) j["trigger"] = t.trigger;
        if (!t.guard.empty()) j["guard"] = t.guard;
        if (!t.delay_ms.is_null()) j["delay_ms"] = t.delay_ms;
    }
    static void from_json(ordered_json const& j, core_fsm::persistence::TransitionDesc& t) {
        j.at("from").get_to(t.from);
        j.at("to").get_to(t.to);
        if (j.contains("trigger")) j.at("trigger").get_to(t.trigger);
        if (j.contains("guard"))   j.at("guard").get_to(t.guard);
        if (j.contains("delay_ms")) {
            j.at("delay_ms").get_to(t.delay_ms);
        }
        // If there's no delay_ms in the file, leave t.delay_ms as null
    }
};

template <>
struct adl_serializer<core_fsm::persistence::FsmDocument> {
    static void to_json(ordered_json& j, core_fsm::persistence::FsmDocument const& d) {
        j = ordered_json{
            {"name", d.name},
            {"comment", d.comment},
            {"inputs", d.inputs},
            {"outputs", d.outputs},
            {"variables", d.variables},
            {"states", d.states},
            {"transitions", d.transitions}
        };
    }
    static void from_json(ordered_json const& j, core_fsm::persistence::FsmDocument& d) {
        // prefer "id" if present, otherwise use "name"
        if (j.contains("id")) {
          j.at("id").get_to(d.name);
        }
        else {
          j.at("name").get_to(d.name);
        }
    
        if (j.contains("comment"))    j.at("comment").get_to(d.comment);
        j.at("inputs")      .get_to(d.inputs);
        j.at("outputs")     .get_to(d.outputs);
        j.at("variables")   .get_to(d.variables);
        j.at("states")      .get_to(d.states);
        j.at("transitions") .get_to(d.transitions);
    }
    
};

} // namespace nlohmann

#endif // CORE_FSM_PERSISTENCE_HPP
