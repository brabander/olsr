
CC ?= gcc
STRIP ?= strip
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib

ifndef OS
all:		help
endif

ifeq ($(OS), linux)
NAME = olsrd_nameservice.so.0.1
MYFLAGS = -Wall -fPIC -D_GNU_SOURCE
LIBS ?= -lc -lm
CFLAGS ?= -O2
CFLAGS += $(MYFLAGS)
INSTALL_LIB = install -D -m 755 $(NAME) $(LIBDIR)/$(NAME);\
	/sbin/ldconfig -n $(LIBDIR)
all: plugin
else
ifeq ($(OS), fbsd)
NAME = olsrd_nameservice.so.0.1
MYFLAGS = -Wall -fPIC
LIBS ?= -lc -lm
CFLAGS ?= -O2
CFLAGS += $(MYFLAGS)
INSTALL_LIB = install -m 755 $(NAME) $(LIBDIR)/$(NAME);\
	/sbin/ldconfig 
all: plugin
else
all: 	help
endif
endif

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))
HDRS = $(wildcard src/*.c)


plugin: $(OBJS)
	$(CC) $(LDFLAGS) $(MYFLAGS) -shared -Wl,-soname,$(NAME) \
	-Wl,--version-script=version-script.txt \
	-o $(NAME) $(OBJS) $(LIBS)

install:
	$(STRIP) $(NAME)
	$(INSTALL_LIB)

clean:
	rm -f $(OBJS) $(NAME)

help:
	@echo
	@echo '***** olsr.org Nameservice Make ****'
	@echo ' You must provide a valid target OS '
	@echo ' by setting the OS variable! Valid  '
	@echo ' target OSes are:                   '
	@echo ' ---------------------------------  '
	@echo ' linux - GNU/Linux                  '
	@echo ' fbsd  - FreeBSD                    '
	@echo ' ---------------------------------  '
	@echo ' Example - build for FreeBSD:       '
	@echo ' gmake OS=fbsd                      '
	@echo '************************************'
	@echo

src/olsrd_plugin.o: $(HDRS)
src/nameservice.o: $(HDRS)
