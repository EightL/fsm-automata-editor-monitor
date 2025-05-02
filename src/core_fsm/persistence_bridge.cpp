#include "persistence.hpp"
#include <fstream>
#include <chrono>
#include <regex>
namespace core_fsm::persistence {

using namespace std::chrono_literals;

// std::chrono::milliseconds 
// parseDelay(nlohmann::json const& j, core_fsm::VarMap const& vars)
// {
//     if (j.is_null()) return 0ms;
//     if (j.is_number_integer()) return std::chrono::milliseconds{j.get<int>()};
//     if (j.is_string()) {
//         auto it = vars.find(j.get<std::string>());
//         if (it != vars.end() && std::holds_alternative<int>(it->second))
//             return std::chrono::milliseconds{std::get<int>(it->second)};
//         throw std::runtime_error("delay_ms refers to unknown/int variable");
//     }
//     throw std::runtime_error("delay_ms must be int or string");
// }

bool loadFile(const std::string& path,
              FsmDocument& out,
              std::string* err) 
{
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
    // Capture the first warning, but don't abort.
    std::string warning;
    auto inputs = j["inputs"].get<std::vector<std::string>>();
    auto is_input = [&](const std::string& s){
      return std::find(inputs.begin(), inputs.end(), s) != inputs.end();
    };

    for (auto& t : j["transitions"]) {
        if (!warning.empty())
            break;               // only capture the first warning

        std::string trig  = t.value("trigger", "");
        std::string guard = t.value("guard", "");

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

        std::regex vo_re(R"(valueof\(\"([^\"]+)\"\))");
        std::smatch m;
        if (std::regex_search(guard, m, vo_re)
            && !is_input(m[1].str()))
        {
            warning = "Guard in transition `" +
                      t["from"].get<std::string>() + "`→`" +
                      t["to"].get<std::string>() +
                      "` references unknown input `" +
                      m[1].str() + "`.";
            break;
        }
    }
    // ─── END SANITY CHECKS ───

    // Always build the document, even if warning is set:
    try {
        out = j.get<FsmDocument>();
    }
    catch (const std::exception& e) {
        if (err) *err = std::string("FSM schema error: ") + e.what();
        return false;  // only bail if the JSON can't map into your DTO
    }

    // If we saw a warning, return true _and_ hand it back:
    if (!warning.empty()) {
        if (err) *err = std::move(warning);
        return true;
    }

    // No warnings → clean load
    return true;
}

bool saveFile(const FsmDocument& doc,
              const std::string& path,
              bool pretty,
              std::string* err) {
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
            out << j.dump() << '\n';
        }
    } catch (const std::exception& e) {
        if (err) *err = e.what();
        return false;
    }
    return true;
}

} // namespace core_fsm::persistence
