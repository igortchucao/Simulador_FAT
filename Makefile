all: main
main: main.o so.o 
		gcc -o main main.o so.o

so.o: main.c so.h
		gcc -c so.c 

main.o: main.c so.h
		gcc -c main.c 

clean:
		rm -rf *.o
mrproper:S
		rm -rf main
