
CC ?= gcc
BINNAME = olsr_switch
INCLUDES += -I./..
CFLAGS += -O2 -g $(INCLUDES) -Wall
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
LIBS += -mno-cygwin -lws2_32 -liphlpapi
endif

all: 	$(OBJS)
	$(CC) $(LDFLAGS) -o $(TOPDIR)$(BINNAME) $(OBJS) $(LIBS)

clean:
	rm -f *.o
	rm -f *~
	rm -f $(TOPDIR)$(BINNAME)
