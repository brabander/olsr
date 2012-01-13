CC ?=	gcc

CFLAGS += -Wall `pkg-config --cflags gtk+-2.0`
LFLAGS= `pkg-config --libs gtk+-2.0` -lm

OBJS=	src/main.o src/interface.o src/callbacks.o src/ipc.o \
	src/packet.o src/nodes.o src/routes.o

all:	olsrd-gui

olsrd-gui:	$(OBJS)
	$(CC) $(LFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(OBJS) olsrd-gui

install:
	mkdir -p ${DESTDIR}/usr/bin
	cp ./olsrd-gui ${DESTDIR}/usr/bin/
