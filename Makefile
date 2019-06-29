all: main
main: main.o so.o 
		gcc -o main main.o so.o

so.o: main.c so.h
		gcc -c grafo.c -W -Wall -pedantic

main.o: main.c so.h
		gcc -c main.c -W -Wall -pedantic

clean:
		rm -rf *.o
mrproper:
		rm -rf main
