CFLAGS=-g
CXXFLAGS=-g

tests: trex_tests.o trex.o
	$(CXX) $(CXXFLAGS) trex_tests.o trex.o && ./a.out

trex.o: trex.c trex.h trex_opcodes.h
	$(CC) $(CFLAGS) -c trex.c

trex_tests.o: trex_tests.cpp trex.h trex_opcodes.h
	$(CXX) $(CXXFLAGS) -std=c++20 -c trex_tests.cpp
