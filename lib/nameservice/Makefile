
CC ?= gcc
STRIP ?= strip
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib
PLUGINFLAGS = -fPIC -DOLSR_PLUGIN -I../../src

ifndef OS
all:		help
endif

ifeq ($(OS), linux)
NAME = olsrd_nameservice.so.0.1
MYFLAGS = -Wall -D_GNU_SOURCE $(PLUGINFLAGS)
LIBS ?= -lc -lm
CFLAGS ?= -O2
CFLAGS += $(MYFLAGS)
INSTALL_LIB = install -D -m 755 $(NAME) $(LIBDIR)/$(NAME);\
	/sbin/ldconfig -n $(LIBDIR)
all: plugin
else
ifeq ($(OS), fbsd)
NAME = olsrd_nameservice.so.0.1
MYFLAGS = -Wall $(PLUGINFLAGS)
LIBS ?= -lc -lm
CFLAGS ?= -O2
CFLAGS += $(MYFLAGS)
INSTALL_LIB = install -m 755 $(NAME) $(LIBDIR)/$(NAME);\
	/sbin/ldconfig 
all: plugin
else
ifeq ($(OS), win32)
NAME ?= olsrd_nameservice.dll
NAMEFLAGS ?= -o $(NAME)
MYFLAGS ?= -g -DWIN32 -mno-cygwin -I../../src/win32 -Wall -Wmissing-prototypes -Wstrict-prototypes $(PLUGINFLAGS)
CFLAGS ?= -O2
CFLAGS += $(MYFLAGS)
LDFLAGS ?= -mno-cygwin -shared
LIBS ?= -lws2_32
INSTALL_LIB = 	cp $(NAME) ../..
all: plugin
else
all: 	help
endif
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
	@echo ' win32 - Windows                    '
	@echo ' ---------------------------------  '
	@echo ' Example - build for FreeBSD:       '
	@echo ' gmake OS=fbsd                      '
	@echo '************************************'
	@echo

src/olsrd_plugin.o: $(HDRS)
src/nameservice.o: $(HDRS)
