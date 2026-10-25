CC=clang++
CFLAGS=-std=c++0x -static -pthread -O3 -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
CFLAGS_SIMPLE=-std=c++0x -pthread -O3 -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING
URCU_REQS = ../obj/urcu.o
INCLUDE=../../../include

obj/catch_test_main.o: catch_test_main.cpp
	$(CC) $(CFLAGS_SIMPLE) -c $<  -o $@

avl_test: avl_test.cpp $(INCLUDE)/* obj/catch_test_main.o $(URCU_REQS) Makefile
	$(CC) $(CFLAGS) avl_test.cpp obj/catch_test_main.o $(URCU_REQS) -o avl_test

tests: avl_test
	./avl_test --benchmark-samples 5

# the same tests, with the copies connected using LLX/SCX instead of TSX
avl_llx_test: avl_test.cpp $(INCLUDE)/* obj/catch_test_main.o $(URCU_REQS) Makefile
	$(CC) $(CFLAGS) -DCOMMIT_TYPE=LLX_SCX_COMMIT avl_test.cpp obj/catch_test_main.o $(URCU_REQS) -o avl_llx_test

tests-llx: avl_llx_test
	./avl_llx_test --benchmark-samples 5

run-tests:
	make clean && make tests

	

clean:
	rm -rf avl_test avl_llx_test