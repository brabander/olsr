
#############################################
#          OLSR.ORG SECURE PLUGIN           #
#############################################


CC ?= gcc
STRIP ?= strip

ifndef OS
all:		help
endif

ifeq ($(OS), linux)
NAME ?= olsrd_secure.so.0.4
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib
MYFLAGS ?= -Wall -fPIC -g #-DDEBUG# Uncomment -g for debugging
LIBS ?= -lc -lssl -lcrypto -lm
CFLAGS ?= -fPIC -g -O2
INSTALL_LIB = install -D -m 755 $(NAME) $(LIBDIR)/$(NAME);\
	/sbin/ldconfig -n $(LIBDIR)

all:	plugin
else
ifeq ($(OS), fbsd)
NAME ?= olsrd_secure.so.0.4
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib
MYFLAGS ?= -Wall -fPIC -g #-DDEBUG# Uncomment -g for debugging
LIBS ?= -lc -lssl -lcrypto -lm
CFLAGS ?= -fPIC -g -O2
INSTALL_LIB = install -m 755 $(NAME) $(LIBDIR)/$(NAME);\
	/sbin/ldconfig

all:	plugin
else
all: 	help
endif
endif

#Sourcefiles
SRCS=	src/olsrd_plugin.c src/olsrd_secure.c

#Objectfiles
OBJS=	src/olsrd_plugin.o src/olsrd_secure.o

#Headerfiles
HDRS=	src/olsrd_plugin.h src/olsrd_secure.h src/olsr_plugin_io.h


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
	@echo '**** olsr.org secure plugin Make ***'
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

src/olsrd_secure.o: $(HDRS)
