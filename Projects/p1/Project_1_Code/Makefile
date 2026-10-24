CC=gcc

all : pseudo-shell
	
pseudo-shell: command.o main.o string_parser.o
	$(CC) -o $@ $^
	gcc -O0 -g -o pseudo-shell command.o main.o string_parser.o
	
# lab2: command.o main.o
# 	$(CC) -o $@ $^
# 	gcc -o lab2 command.o main.o

main.o: main.c
	$(CC) -c $^
	
string_parser.o: string_parser.c string_parser.h
	$(CC) -c $<

command.o: command.c command.h
	$(CC) -c $<
	
clean:
	rm -f main.o command.o string_parser.o lab2
	
clean:
	rm -f core *.o lab2
