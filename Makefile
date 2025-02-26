CFLAGS=-g
CXXFLAGS=-g

trex_tests: trex_tests.o trex_exec.o trex_verify.o
	$(CXX) $(CXXFLAGS) trex_tests.o trex_exec.o trex_verify.o -o trex_tests

trex_exec.o: trex_exec.c trex.h trex_opcodes.h
	$(CC) $(CFLAGS) -c trex_exec.c

trex_verify.o: trex_verify.c trex_impl.h trex.h trex_opcodes.h
	$(CC) $(CFLAGS) -c trex_verify.c

trex_tests.o: trex_tests.cpp trex.h trex_opcodes.h
	$(CXX) $(CXXFLAGS) -std=c++20 -c trex_tests.cpp

check: trex_tests
	./trex_tests

clean:
	$(RM) trex_tests.o trex_exec.o trex_verify.o

.PHONY: check distcheck
