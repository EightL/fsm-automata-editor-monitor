/**
 * @file   snapshot.hpp
 * @brief  Provides a utility to serialize the current state of a
 *         core_fsm::Automaton into a JSON “snapshot” message.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */

#pragma once

#include "automaton.hpp"
#include <nlohmann/json.hpp>
#include <map>

namespace core_fsm {

/**
 * @brief Construct a JSON “state” message from an Automaton.
 *
 * This function collects the FSM’s current inputs, variables, and outputs,
 * sorts them into std::map for stable ordering, and packages them along
 * with the current state name, sequence number, and timestamp into a
 * nlohmann::json object of the form:
 * ```
 * {
 *   "type":    "state",
 *   "seq":     <seq>,
 *   "ts":      <now_ms>,
 *   "state":   <currentState>,
 *   "inputs":  { ... },
 *   "vars":    { ... },
 *   "outputs": { ... }
 * }
 * ```
 *
 * @param fsm     The Automaton instance to snapshot.
 * @param seq     Monotonic sequence number for this snapshot.
 * @param now_ms  Timestamp in milliseconds since the UNIX epoch.
 * @return        A JSON object representing the FSM snapshot.
 */
inline nlohmann::json
makeSnapshot(core_fsm::Automaton const& fsm,
            std::uint64_t seq,
            std::int64_t  now_ms)
{
    // Convert unordered_maps to ordered maps for deterministic JSON
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

} // namespace core_fsm
