
#############################################
# EXAMPLE MAKEFILE TO CREATE A OLSRD PLUGIN #
#############################################

#Alter this file to fit your needs

CC ?= gcc
NAME ?= olsrd_power.so.0.3
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib
# -fPIC creates position independent code
MYFLAGS ?= -Wall -fPIC -g # Uncomment -g for debugging
LIBS ?= -lc -lm

#Sourcefiles
#add yours here
SRCS=	src/olsrd_plugin.c src/olsrd_power.c

#Objectfiles
#add yours here
OBJS=	src/olsrd_plugin.o src/olsrd_power.o

#Headerfiles
#add yours here
HDRS=	src/olsrd_plugin.h src/olsrd_power.h

# -Wl passes options to the linker(-soname,....)
# Use -lc to link it against C library
# not 100% sure if this is neccesary

all: plugin

plugin: $(OBJS)
	$(CC) $(LDFLAGS) $(MYFLAGS) -g -shared -Wl,-soname,$(NAME) \
	-o $(NAME) $(OBJS) $(LIBS)


install:
	install -D -m 755 $(NAME) $(LIBDIR)/$(NAME)
	/sbin/ldconfig -n $(LIBDIR)

clean:
	rm -f $(OBJS) $(NAME)


src/olsrd_plugin.o: $(HDRS)

src/olsrd_power.o: $(HDRS)

