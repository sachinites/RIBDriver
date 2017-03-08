CC=gcc
CFLAGS=-g -Wall
LIBS=-lpthread
OBJ=threadApi.o
TARGET:exe
%.o:%.c
	${CC} ${CFLAGS} -c -I . $<
exe: ${OBJ} rt_test.o
	${CC} ${CFLAGS}  rt_test.o ${OBJ} -o exe ${LIBS}
rt_test.o:rt_test.c
	${CC} ${CFLAGS} -c rt_test.c -o rt_test.o
clean:
	rm *.o
	rm exe

