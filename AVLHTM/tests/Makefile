CC=clang++
CFLAGS=-std=c++0x -static -pthread -O3 -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
URCU_REQS = ../obj/urcu.o

obj/catch_test_main.o: catch_test_main.cpp
	$(CC) $(CFLAGS) -c $<  -o $@

avl_test: avl_test.cpp ../include/* obj/catch_test_main.o $(URCU_REQS)
	$(CC) $(CFLAGS) avl_test.cpp obj/catch_test_main.o $(URCU_REQS) -o avl_test

tests: avl_test
	./avl_test --benchmark-samples 5

run-tests:
	make clean && make tests

	

clean:
	rm -rf avl_test