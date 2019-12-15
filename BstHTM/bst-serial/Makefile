CC=clang++
CFLAGS=-std=c++0x -pthread -O3 -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING

catch_test_main.o: catch_test_main.cpp
	$(CC) $(CFLAGS) -c $<  -o $@

bst_test: bst_test.cpp ../include/catch.hpp catch_test_main.o 
	$(CC) $(CFLAGS) bst_test.cpp catch_test_main.o  -o bst_test

tests: bst_test
	./bst_test --benchmark-samples 5
clean: 
	rm bst_test
