
# You must be running 64-bit OS to generate 64-bit objects!

CC=gcc
LD=ld

sparc: mynull64 mynull32

ia32: mynullx86

mynull64: mynull.c
	${CC} -c -o mynull64.o -D_KERNEL -xO2 -xarch=v9a mynull.c
	${LD} -r -o mynull64 mynull64.o

mynull32: mynull.c
	${CC} -c -o mynull32.o -D_KERNEL -xO2 -xarch=v8plusa mynull.c
	${LD} -r -o mynull32 mynull32.o

mynullx86: mynull.c
	${CC} -c -o mynullx86.o -D_KERNEL -xO2 mynull.c
	${LD} -r -o mynullx86 mynullx86.o

clean:
	rm -f mynull64 mynull32 mynull64.o mynull32.o mynullx86 mynullx86.o
