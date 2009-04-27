SRC +=	$(wildcard ./*.c)

OBJS = $(SRC:.c=.o)

CC = gcc
CFLAGS = -c -g0 -Os -Wall -Werror
LFLAGS = -Wall

.c.o:
	${CC} ${CFLAGS} -o $@ $^

all: txtinfo-sh

txtinfo-sh:	${OBJS}
	${CC} -o ./$@ ${LFLAGS} ${OBJS}

clean:
	rm -f ${OBJS} ./txtinfo-sh

