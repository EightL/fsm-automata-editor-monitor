// context.hpp  – small bag of references handed to guards/actions
#pragma once
#include <unordered_map>
#include "variable.hpp"

namespace core_fsm {

struct Context {
    std::unordered_map<std::string, Variable>& vars;
    std::unordered_map<std::string, std::string>& inputs;   // last known
    std::unordered_map<std::string, std::string>& outputs;  // last sent
    std::chrono::steady_clock::time_point stateSince;       // set by Automaton

    template<class T>
    void setVar(const std::string& n, T v) {
        vars.at(n).set(v);
    }
    const std::string& input(const std::string& n) const { return inputs.at(n); }
    const std::string& output(const std::string& n) const { return outputs.at(n); }

    std::chrono::milliseconds elapsed() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - stateSince);
    }
    void emit(const std::string& name, const std::string& val) {
        outputs[name] = val;
    }
};

} // namespace core_fsm
