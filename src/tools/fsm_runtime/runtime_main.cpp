
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <QCoreApplication>

#include "../../external/nlohmann/json.hpp" // nlohmann::json

#include "../../core_fsm/automaton.hpp"
#include "../../core_fsm/context.hpp"
#include "../../core_fsm/persistence.hpp"
#include "../../core_fsm/state.hpp"
#include "../../core_fsm/transition.hpp"
#include "../../core_fsm/variable.hpp"
#include "../../core_fsm/io_bridge/udp_channel.hpp"

using namespace std::chrono_literals;
using core_fsm::Automaton;
using nlohmann::json;

// -----------------------------------------------------------------------------
// Helper – *very* small subset of the inscription language
// -----------------------------------------------------------------------------
namespace {

// Map textual variable‑type → enum
static core_fsm::Variable::Type mapVarType(const std::string& t) {
    if (t == "int")   return core_fsm::Variable::Type::Int;
    if (t == "float") return core_fsm::Variable::Type::Double;
    return core_fsm::Variable::Type::String;
}

// Build an Automaton from the persistence DTO (limited subset).
static void buildFromDocument(const core_fsm::persistence::FsmDocument& doc, 
                              core_fsm::Automaton& fsm)
{

    // 1) Variables ------------------------------------------------------------
    for (const auto& v : doc.variables) {
        // Determine the enum for the variable’s declared type
        auto varType = mapVarType(v.type);
    
        // Convert the JSON init value into core_fsm::Value
        core_fsm::Value initVal;
        if (v.init.is_number_integer()) {
            initVal = v.init.get<int>();
        }
        else if (v.init.is_number_float()) {
            initVal = v.init.get<double>();
        }
        else {
            // fallback to string
            initVal = v.init.get<std::string>();
        }
    
        // Now we can construct the Variable
        fsm.addVariable({ v.name, varType, std::move(initVal) });
    }

    // 2) States ---------------------------------------------------------------
    // We cannot execute arbitrary C(++) snippets yet.  For P4 we only honour
    //   output("name", <number>)
    // inside the on‑enter action so the GUI at least sees ‘out’ toggling.

    for (const auto& st : doc.states) {
        const std::string enterSrc = st.onEnter; // capture by value into lambda

        // capture both the onEnter source and the state name
        fsm.addState(core_fsm::State{
            st.id,
            [enterSrc, name = st.id](core_fsm::Context& ctx){
                static const std::regex re(R"(output\(\"([^\"]+)\",\s*([0-9]+)\))");
                std::smatch m;
                if (std::regex_search(enterSrc, m, re)) {
                    ctx.output(m[1].str(), m[2].str());
                }
                // Log the state change by name (no ctx.state member)
                std::cout << "{\"type\":\"state\",\"state\":\"" << name << "\"}\n";
            }
        }, st.initial);
    }

    // Build quick lookup table state‑name → index
    std::unordered_map<std::string,std::size_t> idx;
    for (std::size_t i = 0; i < doc.states.size(); ++i)
        idx.emplace(doc.states[i].id, i);

    // 3) Transitions ----------------------------------------------------------
    
    // Build a lookup of variable init‐values so we can resolve string delays
    std::unordered_map<std::string,int> varInit;
    for (const auto& v : doc.variables) {
        if (v.init.is_number_integer())
            varInit[v.name] = v.init.get<int>();
        else if (v.init.is_number_float())
            varInit[v.name] = static_cast<int>(v.init.get<double>());
    }

    for (const auto& tr : doc.transitions) {
        std::chrono::milliseconds delay{0};
        if (tr.delay_ms.is_number_integer()) {
            delay = std::chrono::milliseconds(tr.delay_ms.get<int>());
        }
        else if (tr.delay_ms.is_string()) {
            auto varName = tr.delay_ms.get<std::string>();
            auto it = varInit.find(varName);
            if (it != varInit.end())
                delay = std::chrono::milliseconds(it->second);
        }
        // else null or unrecognized → leave at 0ms

        fsm.addTransition(
            core_fsm::Transition(
                tr.trigger,         // input name
                tr.guard,           // guard expression (string)
                delay,              // delay in ms
                idx.at(tr.from),    // source-state index
                idx.at(tr.to)       // destination-state index
            )
        );
    }

}

}// namespace ------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Ctrl‑C handling – set an atomic flag from signal‑handler context
// -----------------------------------------------------------------------------
static std::atomic_bool g_stop{false};
static void onSigInt(int){ g_stop = true; }

// -----------------------------------------------------------------------------
// MAIN
// -----------------------------------------------------------------------------
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const std::string fsmPath  = (argc > 1 ? argv[1] : "../examples/TOF.fsm.json");
    const std::string bindAddr = (argc > 2 ? argv[2] : "0.0.0.0:45454");
    const std::string peerAddr = (argc > 3 ? argv[3] : "127.0.0.1:45455");

    // 1) Load & build ---------------------------------------------------------
    core_fsm::persistence::FsmDocument doc;
    std::string err;
    if (!core_fsm::persistence::loadFile(fsmPath, doc, &err)) {
        std::cerr << "[fsm_runtime] ERROR: cannot load ‘" << fsmPath << "’ – " << err << "\n";
        return 1;
    }

    core_fsm::Automaton fsm;
    buildFromDocument(doc, fsm);

    // 2) Networking -----------------------------------------------------------
    auto chan = std::make_shared<io_bridge::UdpChannel>(bindAddr, peerAddr);
    fsm.attachChannel(chan);   // Automaton will take care of state broadcasts

    // 3) Run interpreter in worker thread ------------------------------------
    std::thread runner([&]{ fsm.run(); });

    // 4) Event loop: forward UDP → injectInput  (+ optional stdin for testing)
    std::signal(SIGINT, onSigInt);

    while (!g_stop) {
        // 4a) UDP -------------------------------------------------------------
        io_bridge::Packet p;
        while (chan->poll(p)) {
            auto j = json::parse(p.json, nullptr, false);
            if (j.is_discarded()) continue;

            const std::string type = j.value("type", "");
            if (type == "inject") {
                fsm.injectInput(j.at("name").get<std::string>(),
                                j.at("value").get<std::string>());
            } else if (type == "shutdown") {
                g_stop = true;
            }
        }

        // 4b) Stdin -----------------------------------------------------------
        std::string line;
        if (std::getline(std::cin, line)) {               // blocks until <Enter>
            const auto pos = line.find(':');
            if (pos != std::string::npos)
                fsm.injectInput(line.substr(0,pos), line.substr(pos+1));
        } else if (std::cin.eof()) {
            // Ctrl-D = graceful shutdown
            g_stop = true;
        }

        std::this_thread::sleep_for(10ms);
    }

    // 5) Graceful shutdown ----------------------------------------------------
    fsm.requestStop();
    runner.join();
    return 0;
}
