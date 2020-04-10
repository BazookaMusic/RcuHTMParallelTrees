CC=clang++
CFLAGS=-std=c++0x -static -pthread -O3 -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
CFLAGS_SIMPLE=-std=c++0x -pthread -O3 -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING
INCLUDE=../include

obj/catch_test_main.o: catch_test_main.cpp
	$(CC) $(CFLAGS_SIMPLE) -c $<  -o $@

map_test: map_test.cpp $(INCLUDE)/* obj/catch_test_main.o  Makefile
	$(CC) $(CFLAGS) map_test.cpp obj/catch_test_main.o c_files/rcu-htm-internal.o nalloc.o -o map_test

tests: map_test.cpp
	./map_test.cpp --benchmark-samples 5

run-tests:
	make clean && make tests

	

clean:
	rm -rf map_test