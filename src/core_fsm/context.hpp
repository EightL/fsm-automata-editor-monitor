// context.hpp  (new or amended)
#pragma once
#include <unordered_map>
#include <string>
#include <variant>
#include <chrono>
#include "variable.hpp"  // Include this to use the Value definition from variable.hpp

namespace core_fsm {


using VarMap = std::unordered_map<std::string, Value>;
using IOMap  = std::unordered_map<std::string, std::string>;
using Clock  = std::chrono::steady_clock;

struct Context {
    VarMap&  vars;
    IOMap&   inputs;      // last known
    IOMap&   outputs;
    Clock::time_point stateSince;


    template<class T>
    void setVar(std::string const& n, T const& v) {
        vars[n] = Value{v};
    }

    // --- helper API expected by the inscription language --------
    template<class T> T getVar(std::string const& n) const;
    bool   defined(std::string const& in) const noexcept {
        return inputs.find(in) != inputs.end();
    }
    std::string valueof(std::string const& in) const {
        auto it = inputs.find(in);
        return it == inputs.end() ? "" : it->second;
    }
    void output(std::string const& out, std::string const& val) {
        outputs[out] = val;
    }
    std::chrono::milliseconds elapsed() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   Clock::now() - stateSince);
    }
};

// ---------- template impl -------------
template<class T>
T Context::getVar(std::string const& n) const {
    auto it = vars.find(n);
    if (it == vars.end()) throw std::runtime_error("var not found");
    return std::get<T>(it->second);
}


}  // namespace core_fsm
