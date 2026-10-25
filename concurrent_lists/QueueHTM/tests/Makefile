CC=clang++
# CATCH_CONFIG_NO_POSIX_SIGNALS: the bundled Catch v2.9.1 does not compile with glibc >= 2.34 otherwise
CFLAGS=-std=c++0x -static -pthread -O3 -g -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING -DCATCH_CONFIG_NO_POSIX_SIGNALS -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
CFLAGSSIMPLE=-std=c++0x -static -pthread -O3 -Wall -Werror -Wextra -DCATCH_CONFIG_ENABLE_BENCHMARKING -DCATCH_CONFIG_NO_POSIX_SIGNALS

INCLUDE=../../../include

obj/catch_test_main.o: catch_test_main.cpp
	mkdir -p obj
	$(CC) $(CFLAGSSIMPLE) -c $<  -o $@

queue_test: queue_test.cpp ../include/queue.hpp $(INCLUDE)/* obj/catch_test_main.o Makefile
	$(CC) $(CFLAGS) queue_test.cpp obj/catch_test_main.o -o queue_test

tests: queue_test
	./queue_test

# the same tests, with the copies connected using LLX/SCX instead of TSX
queue_llx_test: queue_test.cpp ../include/queue.hpp $(INCLUDE)/* obj/catch_test_main.o Makefile
	$(CC) $(CFLAGS) -DCOMMIT_TYPE=LLX_SCX_COMMIT queue_test.cpp obj/catch_test_main.o -o queue_llx_test

tests-llx: queue_llx_test
	./queue_llx_test

run-tests:
	make clean && make tests

clean:
	rm -rf queue_test obj queue_llx_test
