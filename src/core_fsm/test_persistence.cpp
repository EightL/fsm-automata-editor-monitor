// test_persistence.cpp
#include <iostream>
#include "persistence.hpp"

int main() {
    core_fsm::persistence::FsmDocument doc;
    std::string err;

    // 1) load the example
    if (!core_fsm::persistence::loadFile("examples/TOF.fsm.json", doc, &err)) {
        std::cerr << "LOAD ERROR: " << err << "\n";
        return 1;
    }
    std::cout << "Loaded OK\n";

    // 2) save it back out
    if (!core_fsm::persistence::saveFile(doc, "out.fsm.json", /*pretty=*/true, &err)) {
        std::cerr << "SAVE ERROR: " << err << "\n";
        return 2;
    }
    std::cout << "Saved to out.fsm.json\n";
    return 0;
}

