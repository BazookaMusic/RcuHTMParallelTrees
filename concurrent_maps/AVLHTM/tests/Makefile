CC=clang++
# CATCH_CONFIG_NO_POSIX_SIGNALS: the bundled Catch v2.9.1 does not compile with glibc >= 2.34 otherwise
CFLAGS=-std=c++0x -static -pthread -O3 -g -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING -DCATCH_CONFIG_NO_POSIX_SIGNALS -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
CFLAGSSIMPLE=-std=c++0x -static -pthread -O3 -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING -DCATCH_CONFIG_NO_POSIX_SIGNALS

INCLUDE=../../../include

obj/catch_test_main.o: catch_test_main.cpp
	mkdir -p obj
	$(CC) $(CFLAGSSIMPLE) -c $<  -o $@

avl_test: avl_test.cpp ../include/avl.hpp $(INCLUDE)/* obj/catch_test_main.o Makefile
	$(CC) $(CFLAGS) avl_test.cpp obj/catch_test_main.o -o avl_test

# correctness tests, throughput tests are run with make throughput
tests: avl_test
	./avl_test "~[tp]"

# the same tests, with the copies connected using LLX/SCX instead of TSX
avl_llx_test: avl_test.cpp ../include/avl.hpp $(INCLUDE)/* obj/catch_test_main.o Makefile
	$(CC) $(CFLAGS) -DCOMMIT_TYPE=LLX_SCX_COMMIT avl_test.cpp obj/catch_test_main.o -o avl_llx_test

tests-llx: avl_llx_test
	./avl_llx_test "~[tp]"

throughput: avl_test
	./avl_test "[tp]"

run-tests:
	make clean && make tests

clean:
	rm -rf avl_test obj avl_llx_test
