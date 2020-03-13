CC=clang++
CFLAGS=-std=c++0x -static -pthread -Og -g -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
CFLAGSSIMPLE=-std=c++0x -static -pthread -O3 -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING

INCLUDE=../../../include

obj/catch_test_main.o: catch_test_main.cpp
	$(CC) $(CFLAGSSIMPLE) -c $<  -o $@

stack_test: ../include/stack.hpp stack_test.cpp $(INCLUDE)/* obj/catch_test_main.o  Makefile
	$(CC) $(CFLAGS) stack_test.cpp obj/catch_test_main.o -o stack_test

tests: stack_test
	./stack_test --benchmark-samples 5

run-tests:
	make clean && make tests

	

clean:
	rm -rf stack_test