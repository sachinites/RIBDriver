CC=gcc
CFLAGS=-g -Wall
LIBS=-lpthread
OBJ=threadApi.o
TARGET:exe
%.o:%.c
	${CC} ${CFLAGS} -c -I . $<
exe: ${OBJ} dr_test.o
	${CC} ${CFLAGS}  dr_test.o ${OBJ} -o exe ${LIBS}
rt_test.o:dr_test.c
	${CC} ${CFLAGS} -c dr_test.c -o dr_test.o
clean:
	rm *.o
	rm exe

