
#############################################
# EXAMPLE MAKEFILE TO CREATE A OLSRD PLUGIN #
#############################################

#Alter this file to fit your needs

CC ?= gcc
NAME ?= olsrd_secure.so.0.4
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib
# -fPIC creates position independent code
MYFLAGS ?= -Wall -fPIC -g #-DDEBUG# Uncomment -g for debugging
LIBS ?= -lc -lssl -lcrypto -lm
CFLAGS ?= -g -O2

#Sourcefiles
#add yours here
SRCS=	src/olsrd_plugin.c src/olsrd_secure.c

#Objectfiles
#add yours here
OBJS=	src/olsrd_plugin.o src/olsrd_secure.o

#Headerfiles
#add yours here
HDRS=	src/olsrd_plugin.h src/olsrd_secure.h src/olsr_plugin_io.h

# -Wl passes options to the linker(-soname,....)
# Use -lc to link it against C library
# not 100% sure if this is neccesary

all: plugin


plugin: $(OBJS)
	$(CC) $(LDFLAGS) $(MYFLAGS) -shared -Wl,-soname,$(NAME) \
	-Wl,--version-script=version-script.txt \
	-o $(NAME) $(OBJS) $(LIBS)


install:
	strip $(NAME)
	install -D -m 755 $(NAME) $(LIBDIR)/$(NAME)
	/sbin/ldconfig -n $(LIBDIR)

clean:
	rm -f $(OBJS) $(NAME)

src/olsrd_plugin.o: $(HDRS)

src/olsrd_secure.o: $(HDRS)
