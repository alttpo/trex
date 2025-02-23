tests: trex_tests.o trex.o
	$(CXX) trex_tests.o trex.o && ./a.out

trex.o: trex.c trex.h trex_opcodes.h
	$(CC) -c trex.c

trex_tests.o: trex_tests.cpp trex.h trex_opcodes.h
	$(CXX) -std=c++20 -c trex_tests.cpp
