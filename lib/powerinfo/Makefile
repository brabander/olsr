
#############################################
# EXAMPLE MAKEFILE TO CREATE A OLSRD PLUGIN #
#############################################

#Alter this file to fit your needs

CC ?= gcc
STRIP ?= strip

ifndef OS
all:		help
endif

ifeq ($(OS), linux)
NAME ?= olsrd_power.so.0.3
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib
# -fPIC creates position independent code
MYFLAGS ?= -Wall -fPIC -g # Uncomment -g for debugging
LIBS ?= -lc -lm
CFLAGS ?= -fPIC -g -O2
INSTALL_LIB = install -D -m 755 $(NAME) $(LIBDIR)/$(NAME);\
	/sbin/ldconfig -n $(LIBDIR)
all:	plugin
else
ifeq ($(OS), fbsd)
NAME ?= olsrd_power.so.0.3
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib
# -fPIC creates position independent code
MYFLAGS ?= -Wall -fPIC -g # Uncomment -g for debugging
LIBS ?= -lc -lm
CFLAGS ?= -fPIC -g -O2
INSTALL_LIB = install -m 755 $(NAME) $(LIBDIR)/$(NAME);\
	/sbin/ldconfig
all:	plugin
else
all: 	help
endif
endif

#Sourcefiles
#add yours here
SRCS=	src/olsrd_plugin.c src/olsrd_power.c

#Objectfiles
#add yours here
OBJS=	src/olsrd_plugin.o src/olsrd_power.o

#Headerfiles
#add yours here
HDRS=	src/olsrd_plugin.h src/olsrd_power.h src/olsr_plugin_io.h

# -Wl passes options to the linker(-soname,....)
# Use -lc to link it against C library
# not 100% sure if this is neccesary

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
	@echo '******* olsr.org powerplugin *******'
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

src/olsrd_plugin.o: $(HDRS)

src/olsrd_power.o: $(HDRS)

