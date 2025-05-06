/**
 * @file   runtime_main.cpp
 * @brief  Implements a standalone runtime for FSM execution from JSON definition files.
 *         Handles loading, parsing, automaton construction, and I/O communication.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */
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
#include <unistd.h>
#include <sys/select.h>

#include <nlohmann/json.hpp>
#include <script_engine.hpp>

#include "../core/automaton.hpp"
#include "../core/context.hpp"
#include "../core/persistence.hpp"
#include "../core/state.hpp"
#include "../core/transition.hpp"
#include "../core/variable.hpp"
#include "../core/io/udp_channel.hpp"

using namespace std::chrono_literals;
using core_fsm::Automaton;
using nlohmann::json;

// -----------------------------------------------------------------------------
// Helper – *very* small subset of the inscription language
// -----------------------------------------------------------------------------
namespace {

/**
 * Maps textual variable type names to core_fsm::Variable::Type enums.
 * Supports "int", "float", and defaults to string for other types.
 * 
 * @param t The textual representation of the variable type
 * @return The corresponding Variable::Type enum value
 */
static core_fsm::Variable::Type mapVarType(const std::string& t) {
    if (t == "int")   return core_fsm::Variable::Type::Int;
    if (t == "float") return core_fsm::Variable::Type::Double;
    return core_fsm::Variable::Type::String;
}

/**
 * Constructs an Automaton from a parsed FSM document.
 * Handles variables, states, and transitions with their guards and actions.
 * 
 * @param doc The parsed FSM document containing the state machine definition
 * @param fsm The Automaton instance to configure
 */
static void buildFromDocument(const core_fsm::persistence::FsmDocument& doc, 
                              core_fsm::Automaton& fsm)
{

    // 1) Variables ------------------------------------------------------------
    for (const auto& v : doc.variables) {
        // Determine the enum for the variable's declared type
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
    using core_fsm::script::engine;
    using core_fsm::script::bindCtx;
    using core_fsm::script::pullBack;

    for (const auto& st : doc.states) {
        const std::string src = st.onEnter;
        const std::string stateId = st.id; // Store the ID locally
        fsm.addState(core_fsm::State{
            stateId,
            [src, stateId](core_fsm::Context& ctx){
                // 1) bind C++ context into JS
                auto& eng = engine();
                bindCtx(eng, ctx);

                // 2) compile & cache the JS action
                static QHash<QString,QJSValue> cache;
                QString key = QString::fromStdString(src);
                QJSValue fn = cache.value(key);
                if (!fn.isCallable()) {
                    // wrap in function to allow multiple statements
                    QString wrap = "(function(){ " + key + "; })";
                    fn = eng.evaluate(wrap);
                    cache.insert(key, fn);
                }

                // 3) execute and pull back changes
                if (fn.isCallable()) fn.call();
                pullBack(eng, ctx);
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
        if (tr.delay_ms.is_string()) {
            // variable‐delay transition
            fsm.addTransition(core_fsm::Transition(
                tr.trigger,
                tr.guard,
                tr.delay_ms.get<std::string>(),   // just the var name
                idx.at(tr.from),
                idx.at(tr.to)
            ));
        }
        else {
            // fixed numeric (or zero) delay
            std::chrono::milliseconds delay{0};
            if (tr.delay_ms.is_number_integer())
                delay = std::chrono::milliseconds(tr.delay_ms.get<int>());
    
            fsm.addTransition(core_fsm::Transition(
                tr.trigger,
                tr.guard,
                delay,                           // fixed ms
                idx.at(tr.from),
                idx.at(tr.to)
            ));
        }
    }
}

/**
 * Checks if stdin has data available to read without blocking.
 * Uses select() with zero timeout to perform a non-blocking poll.
 * 
 * @return true if data is available on stdin, false otherwise
 */
bool stdinHasData()
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    timeval tv{0,0};  // zero-timeout = non-blocking poll
    return select(STDIN_FILENO+1, &rfds, nullptr, nullptr, &tv) > 0;
}

}// namespace ------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Ctrl‑C handling – set an atomic flag from signal‑handler context
// -----------------------------------------------------------------------------

/**
 * Global flag for graceful shutdown, modified by signal handler.
 * Using atomic_bool to ensure thread-safe access.
 */
static std::atomic_bool g_stop{false};

/**
 * Signal handler for SIGINT (Ctrl+C).
 * Sets the global atomic flag to indicate that the program should terminate.
 * 
 * @param signal The signal number (unused)
 */
static void onSigInt(int){ g_stop = true; }

// -----------------------------------------------------------------------------
// MAIN
// -----------------------------------------------------------------------------

/**
 * Main entry point for the FSM runtime.
 * Loads an FSM definition, constructs the automaton, and runs it with IO handling.
 * 
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return 0 on success, 1 on error
 */
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const std::string fsmPath  = (argc > 1 ? argv[1] : "../examples/TOF.fsm.json");
    const std::string bindAddr = (argc > 2 ? argv[2] : "0.0.0.0:45454");
    const std::string peerAddr = (argc > 3 ? argv[3] : "127.0.0.1:45455");

    // 1) Load & build ---------------------------------------------------------
    // Parse the JSON FSM definition and construct the automaton
    core_fsm::persistence::FsmDocument doc;
    std::string err;
    if (!core_fsm::persistence::loadFile(fsmPath, doc, &err)) {
        std::cerr << "[fsm_runtime] ERROR: cannot load '" << fsmPath << "' – " << err << "\n";
        return 1;
    }

    core_fsm::Automaton fsm;
    buildFromDocument(doc, fsm);

    // 2) Networking -----------------------------------------------------------
    // Set up UDP communication channel for remote control and monitoring
    auto chan = std::make_shared<io_bridge::UdpChannel>(bindAddr, peerAddr);
    fsm.attachChannel(chan);   // Automaton will take care of state broadcasts

    // 3) Run interpreter in worker thread ------------------------------------
    // Start FSM execution in a separate thread
    std::thread runner([&]{ fsm.run(); });

    // 4) Event loop: forward UDP → injectInput  (+ optional stdin for testing)
    // Set up signal handling for graceful termination
    std::signal(SIGINT, onSigInt);

    while (!g_stop) {
        // 4a) UDP -------------------------------------------------------------
        // Process incoming UDP packets (remote inputs and commands)
        io_bridge::Packet p;
        while (chan->poll(p)) {
            auto j = json::parse(p.json, nullptr, false);
            if (j.is_discarded()) continue;

            const std::string type = j.value("type", "");
            if (type == "inject") {
                fsm.injectInput(j.at("name").get<std::string>(),
                                j.at("value").get<std::string>());
            } 
            else if (type == "setVar") {
                fsm.setVariable(j.at("name").get<std::string>(), j.at("value").get<std::string>());
            }
            else if (type == "shutdown") {
                g_stop = true;
            }
        }

        // 4b) Stdin -----------------------------------------------------------
        // Allow local input injection via terminal for testing
        if (stdinHasData()) {
            std::string line;
            std::getline(std::cin, line);
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
    // Stop the FSM and wait for the worker thread to complete
    fsm.requestStop();
    runner.join();
    return 0;
}
