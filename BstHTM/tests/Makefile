CC=clang++
CFLAGS=-std=c++0x -pthread -O3 -Wall -Werror -Wextra

obj/catch_test_main.o: catch_test_main.cpp
	$(CC) $(CFLAGS) -c $<  -o $@

bst_test: bst_test.cpp ../include/* obj/catch_test_main.o 
	$(CC) $(CFLAGS) bst_test.cpp obj/catch_test_main.o  -o bst_test

tests: bst_test
	./bst_test
