CC=clang++

CFLAGS=-std=c++0x -static -pthread -O3 -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING -Wl,--whole-archive -lpthread -Wl,--no-whole-archive 
CFLAGS_SIMPLE=-std=c++0x -pthread -O3 -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING
INCLUDE=../include

MAP_IMPL=c_files/rcu-htm-internal.o

obj/catch_test_main.o: catch/catch_test_main.cpp
	$(CC) $(CFLAGS_SIMPLE) -c $<  -o $@

obj/rcu-htm-internal.o: c_files/rcu-htm-internal.c
	clang -O3 -static -pthread  -DMAP_KEY_TYPE_INT -c  $< -o $@

obj/avl-rcu-htm-internal.o: c_files/avl-rcu-htm-internal.c
	clang -O3 -static -pthread  -DMAP_KEY_TYPE_INT -c  $< -o $@

obj/nalloc.o: allocator/allocator.c
	clang -O3 -static -pthread -c  $< -o $@

or_alloc: allocator/nalloc_prealloc.c
	clang -O3 -static -pthread -c  $< -o obj/nalloc.o

bst_manual_test: bst_manual_test.cpp $(INCLUDE)/* obj/catch_test_main.o  Makefile  obj/nalloc.o obj/rcu-htm-internal.o
	$(CC) $(CFLAGS) bst_manual_test.cpp obj/catch_test_main.o obj/rcu-htm-internal.o obj/nalloc.o -o bst_manual_test

avl_manual_test: bst_manual_test.cpp $(INCLUDE)/* obj/catch_test_main.o  Makefile  obj/nalloc.o obj/avl-rcu-htm-internal.o 
	$(CC) $(CFLAGS) bst_manual_test.cpp obj/catch_test_main.o obj/avl-rcu-htm-internal.o obj/nalloc.o -o avl_manual_test

tests: bst_manual_test.cpp
	./bst_manual_test.cpp --benchmark-samples 5


run-tests:
	make clean && make tests

	

clean:
	rm -rf bst_manual_test obj/nalloc.o $(MAP_IMPL) avl_manual_test obj/avl-rcu-htm-internal.o

deploy:
	make clean && make bst_manual_test && make avl_manual_test  && scp -i ~/.ssh/id_rsa bst_manual_test  sdrag@scirouter.cslab.ece.ntua.gr:/home/users/sdrag/thesis