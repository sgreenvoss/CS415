CC=gcc

all : part1 part2 part3 part4
	
part1: part1.o
	$(CC) -o $@ $^	

part1.o: part1.c
	$(CC) -c $^

part2: part2.o
	$(CC) -o $@ $^	

part2.o: part2.c
	$(CC) -c $^

part3: part3.o
	$(CC) -g -o $@ $^	

part3.o: part3.c
	$(CC) -c $^

part4: part4.o
	$(CC) -g -o $@ $^	

part4.o: part4.c
	$(CC) -c $^

iobound.o: iobound.c
	$(CC) -c $^
	
cpubound.o: cpubound.c
	$(CC) -c $<

clean:
	rm -f *.o part1 part2 part3 part4
