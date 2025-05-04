// context.hpp
#pragma once
#include <unordered_map>
#include <string>
#include <variant>
#include <chrono>
#include "variable.hpp"     // for core_fsm::Value
#include <stdexcept>

namespace core_fsm {

// Now hold a reference to the real Variable map:
using VarMap = std::unordered_map<std::string, Variable>;
using IOMap  = std::unordered_map<std::string, std::string>;
using Clock  = std::chrono::steady_clock;

struct Context {
    VarMap&  vars;            // <-- reference into Automaton::m_vars
    IOMap&   inputs;
    IOMap&   outputs;
    Clock::time_point stateSince;

    // new ctor binds directly into the real map
    Context(VarMap& vars_,
            IOMap& inputs_,
            IOMap& outputs_,
            Clock::time_point since_)
      : vars(vars_)
      , inputs(inputs_)
      , outputs(outputs_)
      , stateSince(since_)
    {}

    // ... your helper API stays unchanged:
    template<class T> 
    void setVar(std::string const& n, T const& v) {
        vars[n].set(Value{v});
    }
    
    template<class T> 
    T getVar(std::string const& n) const {
        auto it = vars.find(n);
        if (it == vars.end()) throw std::runtime_error("var not found");
        return std::get<T>(it->second.value());
    }
    
    bool defined(std::string const& in) const noexcept {
        return inputs.find(in) != inputs.end();
    }
    
    std::string valueof(std::string const& in) const {
        auto it = inputs.find(in);
        return it == inputs.end() ? "" : it->second;
    }
    
    void output(std::string const& name, std::string const& val) {
        outputs[name] = val;
    }
    
    std::chrono::milliseconds elapsed() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - stateSince);
    }
};

}  // namespace core_fsm