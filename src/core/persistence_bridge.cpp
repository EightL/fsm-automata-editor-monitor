/**
 * @file   persistence_bridge.cpp
 * @brief  Implements loadFile() and saveFile() for FSM JSON persistence.
 *
 * Reads a JSON file into an FsmDocument (with basic schema & semantic checks),
 * and writes an FsmDocument back out as JSON (optionally pretty-printed).
 *
 * @author Your Name
 * @date   2025-05-06
 */

#include "persistence.hpp"
#include <fstream>
#include <chrono>
#include <regex>

namespace core_fsm::persistence {

/**
 * Load an FSM from a JSON file.
 *
 * Opens `path`, parses into an ordered_json, performs sanity checks
 * (unknown triggers, guards without inputs, etc.), then converts to
 * FsmDocument.  
 *
 * @param path  File path to read.
 * @param out   Output FsmDocument.
 * @param err   Optional out-param for error or warning messages.
 * @return      False on fatal I/O or parse errors; true if loaded
 *              (even with non-fatal warnings).
 */
bool loadFile(const std::string& path,
            FsmDocument& out,
            std::string* err)
{
    // 1) Open & parse JSON
    std::ifstream in(path);
    if (!in.is_open()) {
        if (err) *err = "Failed to open file: " + path;
        return false;
    }

    nlohmann::ordered_json j;
    try {
        in >> j;
    }
    catch (const std::exception& e) {
        if (err) *err = std::string("JSON parse error: ") + e.what();
        return false;
    }

    // ─── SANITY CHECKS → WARNINGS ───
    std::string warning;

    // collect declared inputs
    auto inputs = j["inputs"].get<std::vector<std::string>>();

    // collect declared variables by name
    std::vector<std::string> variables;
    for (auto const& v : j["variables"]) {
        variables.push_back(v["name"].get<std::string>());
    }

    auto is_input = [&](const std::string& s) {
        return std::find(inputs.begin(), inputs.end(), s) != inputs.end();
    };
    auto is_symbol = [&](const std::string& s) {
        return is_input(s)
            || std::find(variables.begin(), variables.end(), s)
                != variables.end();
    };

    for (auto& t : j["transitions"]) {
        if (!warning.empty()) break;

        auto trig  = t.value("trigger", std::string{});
        auto guard = t.value("guard",   std::string{});

        if (!guard.empty() && trig.empty()) {
            warning = "Transition `" +
                    t["from"].get<std::string>() + "`→`" +
                    t["to"].get<std::string>() +
                    "` has a guard but no trigger.";
            break;
        }

        if (!trig.empty() && !is_input(trig)) {
            warning = "Unknown trigger `" + trig +
                    "` in transition `" +
                    t["from"].get<std::string>() + "`→`" +
                    t["to"].get<std::string>() +
                    "`; must be one of: " + j["inputs"].dump();
            break;
        }

        // look for valueof("...") usages in guard
        static const std::regex vo_re(R"(valueof\(\"([^\"]+)\"\))");
        std::smatch m;
        if (std::regex_search(guard, m, vo_re) && !is_symbol(m[1].str())) {
            warning = "Guard in transition `" +
                    t["from"].get<std::string>() + "`→`" +
                    t["to"].get<std::string>() +
                    "` references unknown symbol `" +
                    m[1].str() + "`; must be one of: " +
                    j["inputs"].dump() + " or " +
                    j["variables"].dump();
            break;
        }
    }
    // ─── END SANITY CHECKS ───

    // 2) Convert JSON → FsmDocument (may throw on schema errors)
    try {
        out = j.get<FsmDocument>();
    }
    catch (const std::exception& e) {
        if (err) *err = std::string("FSM schema error: ") + e.what();
        return false;
    }

    // 3) If we saw only a warning, return it (still a success)
    if (!warning.empty()) {
        if (err) *err = std::move(warning);
        return true;
    }

    // 4) All good
    return true;
}

/**
 * Save an FSM document to a JSON file.
 *
 * Serializes `doc` into ordered_json and writes it out to `path`.
 * If `pretty==true`, uses 4-space indentation.
 *
 * @param doc     Source FsmDocument.
 * @param path    File path to write.
 * @param pretty  Pretty-print flag.
 * @param err     Optional out-param for error messages.
 * @return        False if the file couldn't be opened or on serialization
 *                errors; true on success.
 */
bool saveFile(const FsmDocument& doc,
            const std::string& path,
            bool pretty,
            std::string* err)
{
    std::ofstream out(path);
    if (!out.is_open()) {
        if (err) *err = "Failed to open file for writing: " + path;
        return false;
    }
    try {
        nlohmann::ordered_json j = doc;
        if (pretty) {
            out << j.dump(4) << '\n';
        } else {
            out << j.dump()   << '\n';
        }
    }
    catch (const std::exception& e) {
        if (err) *err = e.what();
        return false;
    }
    return true;
}

} // namespace core_fsm::persistence
