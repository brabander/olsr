#######################################
# HTTPINFO PLUGIN BY ANDREAS TØNNESEN #
#######################################


OS ?= linux
CC ?= gcc
STRIP ?= strip
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib
# -fPIC creates position independent code


ifeq ($(OS), linux)
NAME ?= olsrd_httpinfo.so.0.1
NAMEFLAGS ?= -Wl,-soname,$(NAME)
CFLAGS ?= -g -O2 -fPIC -Dlinux -Wall -Wmissing-prototypes -Wstrict-prototypes
LDFLAGS ?= -fPIC -shared
LIBS ?= -lc -lm
INSTALL_LIB = 	install -D -m 755 $(NAME) $(LIBDIR)/$(NAME);\
		/sbin/ldconfig -n $(LIBDIR)
else
ifeq ($(OS), win32)
NAME ?= olsrd_httpinfo.dll
NAMEFLAGS ?= -o $(NAME)
CFLAGS ?= -g -O2 -fPIC -DWIN32 -mno-cygwin -I../../src/win32 -Wall -Wmissing-prototypes -Wstrict-prototypes
LDFLAGS ?= -mno-cygwin -shared
LIBS ?= -lws2_32
COMPATOBJ = ../../src/win32/compat.o
INSTALL_LIB = 	cp $(NAME) ../..
endif
endif

#Sourcefiles
#add yours here
SRCS=	src/olsrd_plugin.c src/olsrd_httpinfo.c

#Objectfiles
#add yours here
OBJS=	src/olsrd_plugin.o src/olsrd_httpinfo.o

#Headerfiles
#add yours here
HDRS=	src/olsrd_plugin.h src/olsrd_httpinfo.h src/olsr_plugin_io.h\
	src/gfx.h

all: plugin


plugin: $(OBJS)
	$(CC) $(LDFLAGS) $(NAMEFLAGS) \
	-Wl,--version-script=version-script.txt \
	-o $(NAME) $(OBJS) $(COMPATOBJ) $(LIBS)


install:
	$(STRIP) $(NAME)
	$(INSTALL_LIB)

clean:
	rm -f $(OBJS) $(NAME)

src/olsrd_plugin.o: $(HDRS)

src/olsrd_httpinfo.o: $(HDRS)
