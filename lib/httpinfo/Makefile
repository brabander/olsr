#######################################
# HTTPINFO PLUGIN BY ANDREAS TØNNESEN #
#######################################

CC ?= gcc
STRIP ?= strip
NAME ?= olsrd_httpinfo.so.0.1
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib
# -fPIC creates position independent code
MYFLAGS ?= -Wall -fPIC -g # Uncomment -g for debugging
LIBS ?= -lc -lm
CFLAGS ?= -g -O2 -Wall -Wmissing-prototypes -Wstrict-prototypes

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

# -Wl passes options to the linker(-soname,....)
# Use -lc to link it against C library
# not 100% sure if this is neccesary

all: plugin


plugin: $(OBJS)
	$(CC) $(LDFLAGS) $(MYFLAGS) -shared -Wl,-soname,$(NAME) \
	-Wl,--version-script=version-script.txt \
	-o $(NAME) $(OBJS) $(LIBS)


install:
	$(STRIP) $(NAME)
	install -D -m 755 $(NAME) $(LIBDIR)/$(NAME)
	/sbin/ldconfig -n $(LIBDIR)

clean:
	rm -f $(OBJS) $(NAME)

src/olsrd_plugin.o: $(HDRS)

src/olsrd_httpinfo.o: $(HDRS)
