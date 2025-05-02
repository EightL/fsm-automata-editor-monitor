#pragma once
#include "automaton.hpp"
#include <nlohmann/json.hpp>

inline nlohmann::json
makeSnapshot(core_fsm::Automaton const& fsm,
             std::uint64_t seq,
             std::int64_t  now_ms)
{
    // turn each unordered_map into a std::map:
    std::map<std::string,std::string> inputs  (fsm.inputs().begin(),  fsm.inputs().end());
    std::map<std::string,std::string> vars    (fsm.vars().begin(),    fsm.vars().end());
    std::map<std::string,std::string> outputs (fsm.outputs().begin(), fsm.outputs().end());

    nlohmann::json j = {
        {"type",    "state"},
        {"seq",     seq},
        {"ts",      now_ms},
        {"state",   fsm.currentState()},
        {"inputs",  inputs},
        {"vars",    vars},
        {"outputs", outputs}
    };
    return j;
}