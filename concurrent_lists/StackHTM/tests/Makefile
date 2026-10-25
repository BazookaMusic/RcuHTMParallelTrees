CC=clang++
# CATCH_CONFIG_NO_POSIX_SIGNALS: the bundled Catch v2.9.1 does not compile with glibc >= 2.34 otherwise
CFLAGS=-std=c++0x -static -pthread -O3 -g -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING -DCATCH_CONFIG_NO_POSIX_SIGNALS -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
CFLAGSSIMPLE=-std=c++0x -static -pthread -O3 -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING -DCATCH_CONFIG_NO_POSIX_SIGNALS

INCLUDE=../../../include

obj/catch_test_main.o: catch_test_main.cpp
	mkdir -p obj
	$(CC) $(CFLAGSSIMPLE) -c $<  -o $@

stack_test: stack_test.cpp ../include/stack.hpp $(INCLUDE)/* obj/catch_test_main.o Makefile
	$(CC) $(CFLAGS) stack_test.cpp obj/catch_test_main.o -o stack_test

tests: stack_test
	./stack_test

run-tests:
	make clean && make tests

clean:
	rm -rf stack_test obj
