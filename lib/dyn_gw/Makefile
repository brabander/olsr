
CC ?= gcc
STRIP ?= strip
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib

ifndef OS
all:		help
endif

ifeq ($(OS), linux)
NAME ?= olsrd_dyn_gw.so.0.3
MYFLAGS ?= -Wall -fPIC -Dlinux -g # Uncomment -g for debugging
LIBS ?= -lpthread -lc -lm
CFLAGS ?= -fPIC -Wall -Wmissing-prototypes -Wstrict-prototypes -g -O2
INSTALL_LIB = install -D -m 755 $(NAME) $(LIBDIR)/$(NAME);\
	      /sbin/ldconfig -n $(LIBDIR)
all:	plugin
else
ifeq ($(OS), fbsd)
NAME ?= olsrd_dyn_gw.so.0.3
MYFLAGS ?= -Wall -fPIC -g # Uncomment -g for debugging
LIBS ?= -lpthread -lc -lm
CFLAGS ?= -fPIC -Wall -Wmissing-prototypes -Wstrict-prototypes -g -O2
INSTALL_LIB = install -m 755 $(NAME) $(LIBDIR)/$(NAME);\
	      /sbin/ldconfig
all:	plugin
else
all: 	help
endif
endif


#Sourcefiles
SRCS=	src/olsrd_plugin.c src/olsrd_dyn_gw.c

#Objectfiles
OBJS=	src/olsrd_plugin.o src/olsrd_dyn_gw.o

#Headerfiles
HDRS=	src/olsrd_plugin.h src/olsrd_dyn_gw.h src/olsr_plugin_io.h

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
	@echo '**** olsr.org dynGW plugin Make ****'
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

src/olsrd_dyn_gw.o: $(HDRS)
