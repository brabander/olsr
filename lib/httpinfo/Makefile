#######################################
# HTTPINFO PLUGIN BY ANDREAS TØNNESEN #
#######################################


CC ?= gcc
STRIP ?= strip
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib
OLSR_LIBDIR ?= ../../src/

ifndef OS
all:		help
endif

ifeq ($(OS), linux)
NAME ?= olsrd_httpinfo.so.0.1
NAMEFLAGS ?= -Wl,-soname,$(NAME)
CFLAGS ?= -g -O2 -fPIC -DOLSR_PLUGIN -Dlinux -Wall -Wmissing-prototypes -Wstrict-prototypes -I$(OLSR_LIBDIR)
LDFLAGS ?= -g -fPIC -shared
LIBS ?= -lc -lm
INSTALL_LIB = 	install -D -m 755 $(NAME) $(LIBDIR)/$(NAME);\
		/sbin/ldconfig -n $(LIBDIR)
all:		plugin
else
ifeq ($(OS), win32)
NAME ?= olsrd_httpinfo.dll
NAMEFLAGS ?= -o $(NAME)
CFLAGS ?= -g -O2 -fPIC -DOLSR_PLUGIN -DWIN32 -mno-cygwin -I../../src/win32 -Wall -Wmissing-prototypes -Wstrict-prototypes -I$(OLSR_LIBDIR)
LDFLAGS ?= -mno-cygwin -shared
LIBS ?= -lws2_32
COMPATOBJ = ../../src/win32/compat.o
INSTALL_LIB = 	cp $(NAME) ../..
all:		plugin
else
ifeq ($(OS), fbsd)
NAME ?= olsrd_httpinfo.so.0.1
NAMEFLAGS ?= -Wl,-soname,$(NAME)
CFLAGS ?= -g -O2 -fPIC -DOLSR_PLUGIN -Wall -Wmissing-prototypes -Wstrict-prototypes -I$(OLSR_LIBDIR)
LDFLAGS ?= -g -fPIC -shared
LIBS ?= -lc -lm
INSTALL_LIB = 	install -m 755 $(NAME) $(LIBDIR)/$(NAME);\
		/sbin/ldconfig
all:		plugin
else
all: 	help
endif
endif
endif

ifneq ($(INCLUDE_SETTINGS), )
	@echo 'WARNING - BUILDING WITH ADMIN INTERFACE!'
CFLAGS += -DINCLUDE_SETTINGS
endif

#Sourcefiles
SRCS=	src/olsrd_plugin.c src/olsrd_httpinfo.c src/admin_interface.c

#Objectfiles
OBJS=	src/olsrd_plugin.o src/olsrd_httpinfo.o src/admin_interface.o

#Headerfiles
HDRS=	src/olsrd_plugin.h src/olsrd_httpinfo.h src/olsr_plugin_io.h\
	src/gfx.h src/html.h src/admin_interface.h src/admin_html.h


plugin: $(OBJS)
	$(CC) $(LDFLAGS) $(NAMEFLAGS) \
	-Wl,--version-script=version-script.txt \
	-o $(NAME) $(OBJS) $(COMPATOBJ) $(LIBS)


install:
	$(STRIP) $(NAME)
	$(INSTALL_LIB)

help:
	@echo
	@echo '***** olsr.org HTTP plugin Make ****'
	@echo ' You must provide a valid target OS '
	@echo ' by setting the OS variable! Valid  '
	@echo ' target OSes are:                   '
	@echo ' ---------------------------------  '
	@echo ' linux - GNU/Linux                  '
	@echo ' win32 - MS Windows                 '
	@echo ' fbsd  - FreeBSD                    '
	@echo ' ---------------------------------  '
	@echo ' Example - build for windows:       '
	@echo ' make OS=win32                      '
	@echo '************************************'
	@echo

clean:
	rm -f $(OBJS) $(NAME)

src/olsrd_plugin.o: $(HDRS)

src/olsrd_httpinfo.o: $(HDRS)

src/admin_interface.o: $(HDRS)
