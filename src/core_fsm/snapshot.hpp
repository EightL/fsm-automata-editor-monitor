#pragma once
#include "automaton.hpp"
#include <nlohmann/json.hpp>

inline nlohmann::json
makeSnapshot(core_fsm::Automaton const& fsm,
             std::uint64_t seq,
             std::int64_t  now_ms)
{
    nlohmann::json j = {
        {"type",  "state"},
        {"seq",   seq},
        {"ts",    now_ms},
        {"state", fsm.currentState()},
        {"inputs",  fsm.inputs()},   // returns std::unordered_map<string,string>
        {"vars",    fsm.vars()},
        {"outputs", fsm.outputs()}
    };
    return j;
}
