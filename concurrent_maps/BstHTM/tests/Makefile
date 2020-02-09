CC=clang++
CFLAGS=-std=c++0x -static -pthread -O3 -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING -Wl,--whole-archive -lpthread -Wl,--no-whole-archive -DHACI3COMP
CFLAGSSIMPLE=-std=c++0x -static -pthread -O3 -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING
URCU_REQS = ../obj/urcu.o

INCLUDE=../../../include

obj/catch_test_main.o: catch_test_main.cpp
	$(CC) $(CFLAGSSIMPLE) -c $<  -o $@

bst_test: bst_test.cpp $(INCLUDE)/* obj/catch_test_main.o $(URCU_REQS) Makefile
	$(CC) $(CFLAGS) bst_test.cpp obj/catch_test_main.o $(URCU_REQS) -o bst_test

tests: bst_test
	./bst_test --benchmark-samples 5

run-tests:
	make clean && make tests

	

clean:
	rm -rf bst_test