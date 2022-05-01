main:
	gcc -c hw3unnamed.c -Wall
	gcc -c hw3named.c -Wall

	gcc -pthread hw3unnamed.o -lrt -o hw3unnamed
	gcc -pthread hw3named.o -lrt -o hw3named
