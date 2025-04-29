CXX = g++
CXXFLAGS = -std=c++17 -Wall -Icore_fsm

SRCS = core_fsm/automaton.cpp
OBJS = $(SRCS:.cpp=.o)

all: main

main: $(OBJS) examples/main.cpp
	$(CXX) $(CXXFLAGS) -o fsm_demo $(OBJS) examples/main.cpp

clean:
	rm -f $(OBJS) fsm_demo
