IDIR=./include
CC=clang++
CFLAGS= -g -std=c++0x -pthread -O3  -Wall -Werror -Wextra -Wpedantic
OBJ=./obj


URCU_LIB_DIR=../../userspaceRCUcpp/src/

INCLUDES = $(wildcard $(IDIR)/*.hpp)


DEPS= obj/urcu.o 


all: avl_test safe_tree_test



obj/catch_test_main.o: catch_test_main.cpp
	$(CC) $(CFLAGS) -c $< -o $@



TEST_MAIN=obj/catch_test_main.o

avl_test: avl_test.cpp obj/catch_test_main.o $(DEPS) $(INCLUDES)
	$(CC) $(CFLAGS) avl_test.cpp  $(TEST_MAIN) $(DEPS) -o avl_test

safe_tree_test: safe_tree_test.cpp obj/catch_test_main.o $(DEPS) $(INCLUDES)
	$(CC) $(CFLAGS) safe_tree_test.cpp  $(TEST_MAIN) $(DEPS) -o safe_tree_test



test: all
	./avl_test -r compact && ./safe_tree_test -r compact
test_safe_tree: safe_tree_test
	./safe_tree_test -r compact




.PHONY: clean

clean:
	rm -f *.o && rm -f $(URCU)/obj/urcu.o