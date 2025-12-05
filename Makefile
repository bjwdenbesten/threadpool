CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -fsanitize=thread 
TARGETS = standard_test ws_test benchmark

.PHONY: all clean test standard workstealing benchmark
all: $(TARGETS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

standard: standard_test

standard_test: standard/threadpool.o tests/standard_test.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

workstealing: ws_test

ws_test: work_stealing/ws_threadpool.o tests/ws_test.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

benchmark: work_stealing/ws_threadpool.o benchmarks/benchmark.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

standard/threadpool.o: standard/threadpool.c standard/threadpool.h
work_stealing/ws_threadpool.o: work_stealing/ws_threadpool.c work_stealing/ws_threadpool.h
tests/standard_test.o: tests/standard_test.c standard/threadpool.h
tests/ws_test.o: tests/ws_test.c work_stealing/ws_threadpool.h
benchmarks/benchmark.o: benchmarks/benchmark.c work_stealing/ws_threadpool.h

test: standard workstealing
	@echo "=== Standard Threadpool Tests ==="
	./standard_test
	@echo ""
	@echo "=== Work-Stealing Threadpool Tests ==="
	./ws_test

clean:
	rm -f $(TARGETS)
	rm -f standard/*.o work_stealing/*.o tests/*.o benchmarks/*.o
