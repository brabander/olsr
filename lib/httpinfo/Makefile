#######################################
# HTTPINFO PLUGIN BY ANDREAS TØNNESEN #
#######################################

CC ?= gcc
STRIP ?= strip
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib
# -fPIC creates position independent code

ifeq ($(OS), win32)
NAME ?= olsrd_httpinfo.dll
NAMEFLAGS ?= -o $(NAME)
CFLAGS ?= -g -O2 -DWIN32 -mno-cygwin -I../../src/win32 -Wall -Wmissing-prototypes -Wstrict-prototypes
LDFLAGS ?= -mno-cygwin -shared
LIBS ?= -lws2_32
COMPATOBJ = ../../src/win32/compat.o
else
NAME ?= olsrd_httpinfo.so.0.1
NAMEFLAGS ?= -Wl,-soname,$(NAME)
CFLAGS ?= -g -O2 -Dlinux -Wall -Wmissing-prototypes -Wstrict-prototypes
LDFLAGS ?= -fPIC -shared
LIBS ?= -lc -lm
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
	install -D -m 755 $(NAME) $(LIBDIR)/$(NAME)
	/sbin/ldconfig -n $(LIBDIR)

clean:
	rm -f $(OBJS) $(NAME)

src/olsrd_plugin.o: $(HDRS)

src/olsrd_httpinfo.o: $(HDRS)
