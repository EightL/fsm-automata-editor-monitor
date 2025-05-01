#include <fstream>
#include "../../external/inja/inja.hpp"  // inja::Environment
#include "../../external/nlohmann/json.hpp" 
#include "../core_fsm/persistence.hpp"          // DTO structs
using nlohmann::json;
using core_fsm::persistence::FsmDocument;

static std::string loadFile(const std::string& p){
    std::ifstream f(p);
    return { std::istreambuf_iterator<char>(f),
             std::istreambuf_iterator<char>() };
}

int main(int argc, char** argv)
{
    if (argc < 3){
        std::cerr << "usage: codegen <fsm.json> <out-dir> [<template-root>]\n";
        return 1;
    }
    const std::string fsmPath = argv[1];
    const std::string outDir  = argv[2];
    // allow caller to override "templates" lookup root via argv[3]
    const std::string tmplRoot =
        (argc >= 4
           ? std::string(argv[3])
           : std::string("templates"));


    /* 1) parse DTO ----------------------------------------------------------*/
    FsmDocument doc;
    std::string err;
    if (!core_fsm::persistence::loadFile(fsmPath, doc, &err)){
        std::cerr << err << '\n';  return 2;
    }

    /* 2) build inja data-model ---------------------------------------------*/
    json ctx;
    ctx["id"] = doc.name;
    ctx["source"] = fsmPath;

    /* variables */
    for (auto const& v : doc.variables){
        json j;
        j["name"] = v.name;
        j["cpp_type"] = (v.type == "int") ? "Int" :
                        (v.type == "float") ? "Double" : "String";
        j["init_literal"] = v.init.dump();
        ctx["variables"].push_back(j);
    }

    /* states */
    size_t idx=0;
    for (auto const& s : doc.states){
        json j;
        j["id"] = s.id;
        j["initial"] = s.initial;
        j["on_enter"] = s.onEnter;
        ctx["states"].push_back(j);
        ctx["stateIndex"][s.id] = idx++;
    }

    /* transitions */
    for (auto const& t : doc.transitions){
        json j;
        j["trigger"] = t.trigger;

        if (!t.guard.empty()) {
            // wrap the raw guard expression in a C++ lambda
            std::string lambda = 
                "[](Context& ctx) { return " + t.guard + "; }";
                j["guard_lambda"] = lambda;
        }
        else {
            j["guard_lambda"] = "nullptr";
        }

        if (t.delay_ms.is_null()) {
            j["delay_ms"] = 0;
            j["delay_is_var"] = false;
          }
        else if (t.delay_ms.is_number_integer()) {
            j["delay_ms"] = t.delay_ms.get<int>();
            j["delay_is_var"] = false;
          }
        else {  // string
            j["delay_ms"]    = t.delay_ms.get<std::string>();
            j["delay_is_var"] = true;
        }

        j["src_index"] = ctx["stateIndex"][t.from];
        j["dst_index"] = ctx["stateIndex"][t.to];
        j["guard_lambda"] = "nullptr";
        ctx["transitions"].push_back(j);
    }

    /* 3) render & dump files -----------------------------------------------*/
    inja::Environment env;
    env.set_trim_blocks(true);
    env.set_lstrip_blocks(true);

    // Note: we render the .hpp first, then the .cpp
    const auto hdr = env.render(loadFile(tmplRoot + "/automaton.hpp.tmpl"), ctx);
    const auto cpp = env.render(loadFile(tmplRoot + "/automaton.cpp.tmpl"), ctx);

    auto name = ctx["id"].get<std::string>();
    std::ofstream(outDir + '/' + name + ".hpp") << cpp;
    std::ofstream(outDir + '/' + name + ".cpp") << hdr;
}