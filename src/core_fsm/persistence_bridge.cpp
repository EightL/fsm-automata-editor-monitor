#include "persistence.hpp"
#include <fstream>
#include <chrono>

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
              std::string* err) {
    std::ifstream in(path);
    if (!in.is_open()) {
        if (err) *err = "Failed to open file: " + path;
        return false;
    }
    try {
        nlohmann::ordered_json j;
        in >> j;
        out = j.get<FsmDocument>();
    } catch (const std::exception& e) {
        if (err) *err = e.what();
        return false;
    }
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
