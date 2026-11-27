CC = gcc

all: lab9 lab9ai

lab9ai: lab9_ai.o
	$(CC) -pthread -lpthread $^ -o $@

lab9_ai.o: lab9_ai.c
	$(CC) -c $<

lab9: lab9.o
	$(CC) -pthread -lpthread $^ -o $@

%.o: %.c
	$(CC) -c $<

clean:
	rm -f core *.o lab9
