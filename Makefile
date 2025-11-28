CC = gcc
CFLAGS = -Wall -fsanitize=thread

main: main.o threadpool.o
	$(CC) $(CFLAGS) main.o threadpool.o -o main

main.o: main.c threadpool.h
	$(CC) $(CFLAGS) -c main.c

threadpool.o: threadpool.c threadpool.h
	$(CC) $(CFLAGS) -c threadpool.c

.PHONY: clean

clean:
	rm -f main *.o
