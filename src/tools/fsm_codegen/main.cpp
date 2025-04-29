// src/tools/fsm_codegen/main.cpp

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <map>
#include <cstdlib>

#include "../../core_fsm/persistence.hpp"    // from core_fsm/persistence.hpp

namespace fs = std::filesystem;
using core_fsm::persistence::loadFile;
using core_fsm::persistence::FsmDocument;

//------------------------------------------------------------------------------
// Emit the single-file C++ interpreter for this FSM.
//------------------------------------------------------------------------------
static void generate_cpp(const FsmDocument& doc, const fs::path& out_file) {
    fs::create_directories(out_file.parent_path());
    std::ofstream ofs(out_file);
    if (!ofs) {
        std::cerr << "Error: cannot open " << out_file << " for writing\n";
        std::exit(1);
    }

    // Includes & usings
    ofs << "#include \"../core_fsm/variable.hpp\"\n";
    ofs << "#include \"../core_fsm/state.hpp\"\n";
    ofs << "#include \"../core_fsm/transition.hpp\"\n";
    ofs << "#include \"../core_fsm/automaton.hpp\"\n";
    ofs << "#include \"../core_fsm/context.hpp\"\n";
    ofs << "#include \"../../external/nlohmann/json.hpp\"\n";  // Add JSON library include
    ofs << "#include <thread>\n";
    ofs << "#include <functional>\n";
    ofs << "#include <iostream>\n";
    ofs << "#include <string>\n";
    ofs << "#include <chrono>\n\n";
    ofs << "using namespace core_fsm;\n";
    ofs << "using namespace std::chrono_literals;\n\n";

    // helper macros so user scripts keep the spec's syntax
    ofs << "// --- helpers injected by code-gen -------------------------\n";
    ofs << "#define output(name,val)   ctx.outputs[name] = std::to_string(val)\n";
    ofs << "#define valueof(name)      ctx.inputs.at(name).c_str()\n";
    ofs << "// -----------------------------------------------------------\n\n";

    // build_fsm()
    ofs << "static std::unique_ptr<Automaton> build_fsm() {\n";
    ofs << "    auto fsm = std::make_unique<Automaton>();\n\n";

    // 1) Variables
    for (auto const& var : doc.variables) {
        // map text → enum literal
        auto enumName = [](std::string t){
            if(t == "int")    return "Int";
            if(t == "float")  return "Float";
            return "String";
        }(var.type);

        ofs << "    fsm->addVariable({ \""
            << var.name << "\", Variable::Type::"
            << enumName << ", "
            << var.init.dump()
            << " });\n";
    }
    ofs << "\n";

    // 2) States
    for (auto const& st : doc.states) {
        ofs << "    fsm->addState(State{\n";
        // State name
        ofs << "        \"" << st.id << "\",\n";
        // Lambda body
        ofs << "        [](Context& ctx) {\n";
        // if (!st.onEnter.empty()) {
        //     ofs << "            " << st.onEnter << ";\n";
            
        //     // Only emit JSON if there's an onEnter action
        //     ofs << "            // emit JSON to stdout via nlohmann::json\n";
        //     ofs << "            nlohmann::json ev = {\n";
        //     // Order matters for token matching: type, state, output
        //     ofs << "                {\"type\", \"state\"},\n";
        //     ofs << "                {\"state\", \"" << st.id << "\"},\n";
        //     ofs << "                {\"output\", { {\"out\", ctx.outputs.at(\"out\")} }}\n";
        //     ofs << "            };\n";
        //     ofs << "            std::cout << ev.dump() << '\\n';\n";
        // }
        if (!st.onEnter.empty()) {
            // execute the user’s action
            ofs << "            " << st.onEnter << ";\n";
            // emit the same JSON as the demo
            ofs << "            std::cout << \"{\\\"type\\\":\\\"state\\\","
                    "\\\"state\\\":\\\"" << st.id << "\\\"}\""
                    " << \"\\n\";\n";
        }
        ofs << "        }\n";
        // close State{ … } and add the initial flag if needed
        ofs << "    }" << (st.initial ? ", true" : "") << ");\n";
    }
    ofs << "\n";

    // Map state‐names → indices
    std::map<std::string,int> idx;
    for (int i = 0; i < (int)doc.states.size(); ++i) 
        idx[doc.states[i].id] = i;

    // 3) Transitions
    for (auto const& tr : doc.transitions) {
        // input name
        std::string inName = tr.trigger.empty() ? "" : tr.trigger;
    
        // guard
        std::string guard = "nullptr";
        if (!tr.guard.empty()) {
            guard = "[&](const GuardCtx& ctx){ return " + tr.guard + "; }";
        }
    
        // delay_ms → delayCode
        std::string delayCode;
        if (tr.delay_ms.is_number_integer()) {
            delayCode = "std::chrono::milliseconds("
                      + std::to_string(tr.delay_ms.get<int>())
                      + ")";
        } else if (tr.delay_ms.is_string()) {
            // try to resolve to a numeric literal at code-gen time
            auto var = tr.delay_ms.get<std::string>();
            auto it  = std::find_if(doc.variables.begin(), doc.variables.end(),
                                    [&](auto& v){ return v.name == var &&
                                                         v.init.is_number();});
            if (it != doc.variables.end())
                delayCode = "std::chrono::milliseconds(" + it->init.dump() + ")";
            else
                delayCode = "0ms"; // fallback – still compiles
        } else {
            delayCode = "0ms";
        }
    
        ofs << "    fsm->addTransition({ \""
            << inName << "\", "
            << guard  << ", "
            << delayCode << ", "
            << idx[tr.from] << ", "
            << idx[tr.to]  << " });\n";
    }
    ofs << "\n";

    ofs << "    return fsm;\n";
    ofs << "}\n\n";

    // main()
    ofs << "int main(int argc, char** argv) {\n";
    ofs << "    auto fsm = build_fsm();\n";
    ofs << "    std::thread runner([&]{ fsm->run(); });\n\n";
    ofs << "    std::string line;\n";
    ofs << "    while (std::getline(std::cin, line)) {\n";
    ofs << "        auto pos = line.find(':');\n";
    ofs << "        if (pos == std::string::npos) continue;\n";
    ofs << "        auto name  = line.substr(0, pos);\n";
    ofs << "        auto value = line.substr(pos + 1);\n";
    ofs << "        fsm->injectInput(name, value);\n";
    ofs << "    }\n\n";
    ofs << "    fsm->requestStop();\n";
    ofs << "    runner.join();\n";
    ofs << "    return 0;\n";
    ofs << "}\n";
}

//------------------------------------------------------------------------------
// Emit a minimal CMakeLists.txt to build and run the above.
//------------------------------------------------------------------------------
static void generate_cmake(const std::string& name, const fs::path& out_dir) {
    fs::create_directories(out_dir);
    std::ofstream ofs(out_dir / "CMakeLists.txt");
    if (!ofs) {
        std::cerr << "Error: cannot open CMakeLists.txt in "
                  << out_dir << "\n";
        std::exit(1);
    }

    ofs << "cmake_minimum_required(VERSION 3.10)\n";
    ofs << "project(" << name << "_generated LANGUAGES CXX)\n\n";
    ofs << "set(CMAKE_CXX_STANDARD 17)\n\n";

    // build the generated interpreter + all core_fsm definitions
    ofs << "add_executable(run_generated\n";
    ofs << "    generated.cpp\n";
    ofs << "    \"${CMAKE_SOURCE_DIR}/../core_fsm/variable.cpp\"\n";
    ofs << "    \"${CMAKE_SOURCE_DIR}/../core_fsm/state.cpp\"\n";
    ofs << "    \"${CMAKE_SOURCE_DIR}/../core_fsm/transition.cpp\"\n";
    ofs << "    \"${CMAKE_SOURCE_DIR}/../core_fsm/automaton.cpp\"\n";
    ofs << "    \"${CMAKE_SOURCE_DIR}/../core_fsm/persistence_bridge.cpp\"\n";
    ofs << ")\n";
    ofs << "target_include_directories(run_generated PRIVATE\n";
    ofs << "    \"${CMAKE_SOURCE_DIR}/../core_fsm\"\n";
    ofs << ")\n";
}

//------------------------------------------------------------------------------
// CLI entry point.
//------------------------------------------------------------------------------
int main(int argc, char** argv) {
    fs::path fsm_file = (argc > 1 ? argv[1] : "examples/TOF5s.fsm");
    fs::path out_dir  = (argc > 2 ? argv[2] : "generated");

    FsmDocument doc;
    std::string err;
    if (!loadFile(fsm_file.string(), doc, &err)) {
        std::cerr << "Failed to load FSM file '" 
                  << fsm_file << "':\n" 
                  << err << "\n";
        return 1;
    }

    generate_cpp(doc, out_dir / "generated.cpp");
    generate_cmake(doc.name, out_dir);

    std::cout << "Code generated in '" << out_dir << "'.\n"
                 "To build & run:\n"
                 "  cd " << out_dir << "\n"
                 "  cmake .\n"
                 "  cmake --build . --target run_generated\n";
    return 0;
}
