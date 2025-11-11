CXX = clang++
CC = clang

CountCalls.so: count_calls.cc
	$(CXX) -fPIC -shared $(shell llvm-config --cxxflags) $< -o $@

test.ll: test.c
	$(CC) -S -emit-llvm -O0 test.c -o test.ll

test_instrumented.ll: test.ll CountCalls.so
	opt -load-pass-plugin=./CountCalls.so -passes="count-calls" test.ll -S -o test_instrumented.ll

test_instrumented: test_instrumented.ll
	$(CC) test_instrumented.ll -o test_instrumented

run: test_instrumented
	./test_instrumented

all: CountCalls.so test_instrumented

clean:
	rm -f CountCalls.so test.ll test_instrumented.ll test_instrumented

.PHONY: all run clean