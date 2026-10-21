CC=clang++
# CATCH_CONFIG_NO_POSIX_SIGNALS: the bundled Catch v2.9.1 does not compile with glibc >= 2.34 otherwise
CFLAGS=-std=c++0x -static -pthread -O3 -g -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING -DCATCH_CONFIG_NO_POSIX_SIGNALS -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
CFLAGSSIMPLE=-std=c++0x -static -pthread -O3 -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING -DCATCH_CONFIG_NO_POSIX_SIGNALS

INCLUDE=../../../include

obj/catch_test_main.o: catch_test_main.cpp
	mkdir -p obj
	$(CC) $(CFLAGSSIMPLE) -c $<  -o $@

btree_test: btree_test.cpp ../include/btree.hpp $(INCLUDE)/* obj/catch_test_main.o Makefile
	$(CC) $(CFLAGS) btree_test.cpp obj/catch_test_main.o -o btree_test

# correctness tests, throughput tests are hidden and run with make throughput
tests: btree_test
	./btree_test

throughput: btree_test
	./btree_test "[tp]"

run-tests:
	make clean && make tests

clean:
	rm -rf btree_test obj
