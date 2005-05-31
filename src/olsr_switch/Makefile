
CC ?= gcc
BINNAME = olsr_switch
INCLUDES += -I./..
CFLAGS += -O2 -g $(INCLUDES) -Wall -MD  -Wmissing-prototypes -Wstrict-prototypes \
	-Wmissing-declarations -Wsign-compare
LDFLAGS +=
TOPDIR = ../../

SRCS =	$(wildcard *.c)
HDRS =	$(wildcard *.h)
OBJS =	$(patsubst %.c,%.o,$(SRCS))


# OS detection
OS =		$(shell sh $(TOPDIR)/make/guess_os.sh)
ifeq ($(OS),UNKNOWN)
all: help
else
ifeq ($(OS),win32)
WIN32 = 1
endif
all: default_target
endif

help:
	@echo 
	@echo '***** olsr.org olsr_switch Make ****'
	@echo ' Automatic detection of your OS     '
	@echo ' failed!                            '
	@echo '************************************'
	@echo


# include dependencies
-include $(SRCS:%.c=%.d)

ifdef WIN32
BINNAME = olsr_switch.exe
CFLAGS += -DWIN32 -mno-cygwin
INCLUDES += -I./../win32/
SRCS +=	$(TOPDIR)src/win32/compat.c
LIBS += -mno-cygwin -lws2_32 -liphlpapi
endif

default_target:	$(OBJS)
	$(CC) $(LDFLAGS) -o $(TOPDIR)$(BINNAME) $(OBJS) $(LIBS)

clean:
	rm -f *.o *.d
	rm -f *~
	rm -f $(TOPDIR)$(BINNAME)
