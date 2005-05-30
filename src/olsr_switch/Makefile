
CC ?= gcc
BINNAME = olsr_switch
INCLUDES += -I./..
CFLAGS = -g $(INCLUDES)
LDFLAGS =
TOPDIR = ../../

SRCS =	$(wildcard *.c)
HDRS =	$(wildcard *.h)
OBJS =	$(patsubst %.c,%.o,$(SRCS))

all: 	$(OBJS)
	$(CC) $(LDFLAGS) -o $(TOPDIR)$(BINNAME) $(OBJS)

clean:
	rm -f *.o
	rm -f *~
	rm -f $(TOPDIR)$(BINNAME)
