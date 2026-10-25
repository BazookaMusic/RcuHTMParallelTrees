CC=clang++
CFLAGS=-std=c++0x -static -pthread -O3 -g -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
CFLAGSSIMPLE=-std=c++0x -static -pthread -Og -g -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING
URCU_REQS = ../obj/urcu.o

INCLUDE=../include

obj/catch_test_main.o: catch_test_main.cpp
	$(CC) $(CFLAGSSIMPLE) -c $<  -o $@

avl_generic_test: $(INCLUDE)/* obj/catch_test_main.o $(URCU_REQS) Makefile
	$(CC) $(CFLAGS) avl_generic_test.cpp obj/catch_test_main.o $(URCU_REQS) -o avl_generic

tests: avl_generic
	./avl_generic --benchmark-samples 5

# the same tests, with the copies connected using LLX/SCX instead of TSX
avl_generic_llx_test: $(INCLUDE)/* obj/catch_test_main.o $(URCU_REQS) Makefile
	$(CC) $(CFLAGS) -DCOMMIT_TYPE=LLX_SCX_COMMIT avl_generic_test.cpp obj/catch_test_main.o $(URCU_REQS) -o avl_generic_llx_test

tests-llx: avl_generic_llx_test
	./avl_generic_llx_test --benchmark-samples 5

run-tests:
	make clean && make tests

	

clean:
	rm -rf avl_generic avl_generic_llx_test