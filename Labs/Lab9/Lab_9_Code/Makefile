CC = gcc

all: lab9

lab9: lab9.o
	$(CC) -pthread -lpthread $^ -o $@

%.o: %.c
	$(CC) -c $<

clean:
	rm -f core *.o lab9