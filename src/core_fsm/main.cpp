// main.cpp
// Demonstration “console interpreter” for P1: builds the TOF5s example,
// runs the FSM in a background thread, reads simple stdin commands,
// and emits JSON‐style state/output events to stdout.
//

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include "variable.hpp"
#include "transition.hpp"
#include "state.hpp"
#include "automaton.hpp"
#include "context.hpp"  // Add this if not already included

using namespace core_fsm;
using namespace std::chrono_literals;

int main() {
    Automaton fsm;

    // 1) Variables
    fsm.addVariable({ "timeout",
                      Variable::Type::Int,
                      5000 });

    // 2) States
    //
    // On entry, print a JSON‐style message with the new state and the 'out' output.
    fsm.addState(State{"IDLE",
        [](Context& ctx){  
            std::cout << R"({"type":"state","state":"IDLE","output":{"out":0}})" "\n";
        }
    }, /* initial = */ true);

    fsm.addState(State{"ACTIVE",
        [](Context& ctx){
            std::cout << R"({"type":"state","state":"ACTIVE","output":{"out":1}})" "\n";
        }
    });

    fsm.addState(State{"TIMING",
        [](Context& ctx){
            // No immediate output on entering TIMING
        }
    });

    // 3) Transitions
    auto guardHigh = [](const GuardCtx& ctx){
        return std::stoi(ctx.inputs.at("in")) == 1;
    };
    auto guardLow = [](const GuardCtx& ctx){
        return std::stoi(ctx.inputs.at("in")) == 0;
    };

    // IDLE --> ACTIVE on in==1, instant
    fsm.addTransition({ "in", guardHigh,  0ms, /* src = */ 0, /* dst = */ 1 });
    // ACTIVE --> TIMING on in==0, instant
    fsm.addTransition({ "in", guardLow,   0ms, /* src = */ 1, /* dst = */ 2 });
    // TIMING --> ACTIVE on in==1, instant
    fsm.addTransition({ "in", guardHigh,  0ms, /* src = */ 2, /* dst = */ 1 });
    // TIMING --> IDLE after timeout ms
    fsm.addTransition({ "" , nullptr,    5000ms, /* src = */ 2, /* dst = */ 0 });

    // 4) Run the FSM in a background thread
    std::thread runner([&]{ fsm.run(); });

    // 5) Interactive console I/O: read lines "inputName:value"
    //    e.g. "in:1" to inject input "in" with value "1"
    std::string line;
    while (std::getline(std::cin, line)) {
        auto sep = line.find(':');
        if (sep == std::string::npos) {
            std::cerr << "ignoring malformed line (expected name:value)\n";
            continue;
        }
        std::string name  = line.substr(0, sep);
        std::string value = line.substr(sep + 1);
        fsm.injectInput(name, value);
    }

    // 6) EOF on stdin => shutdown
    fsm.requestStop();
    runner.join();
    return 0;
}
