CC=gcc

all : part1
	
part1: part1.o
	$(CC) -o $@ $^	

part1.o: part1.c
	$(CC) -c $^

iobound.o: iobound.c
	$(CC) -c $^
	
cpubound.o: cpubound.c
	$(CC) -c $<

clean:
	rm -f *.o part1
