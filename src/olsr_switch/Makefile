
CC ?= gcc
BINNAME = olsr_switch
INCLUDES += -I./..
CFLAGS += -O2 -g $(INCLUDES)
LDFLAGS +=
TOPDIR = ../../

SRCS =	$(wildcard *.c)
HDRS =	$(wildcard *.h)
OBJS =	$(patsubst %.c,%.o,$(SRCS))

ifdef WIN32
BINNAME = olsr_switch.exe
CFLAGS += -DWIN32 -mno-cygwin
INCLUDES += -I./../win32/
SRCS +=	$(TOPDIR)src/win32/compat.c
OBJS +=	$(TOPDIR)src/win32/compat.o
LIBS += -mno-cygwin -lws2_32 -liphlpapi
endif

all: 	$(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -o $(TOPDIR)$(BINNAME) $(OBJS)

clean:
	rm -f *.o
	rm -f *~
	rm -f $(TOPDIR)$(BINNAME)
