
#############################################
#         DOT DRAW OLSR.ORG PLUGIN          #
#############################################


CC ?= gcc
STRIP ?= strip
PLUGINFLAGS = -fPIC -DOLSR_PLUGIN -I../../src/

ifndef OS
all:		help
endif


ifeq ($(OS), linux)
NAME ?= olsrd_dot_draw.so.0.3
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib
MYFLAGS ?= -Wall -fPIC -g # Uncomment -g for debugging
LIBS ?= -lc -lm
CFLAGS ?= -g -O2  -Wall -Wmissing-prototypes -Wstrict-prototypes
INSTALL_LIB = 	install -D -m 755 $(NAME) $(LIBDIR)/$(NAME);\
	/sbin/ldconfig -n $(LIBDIR)
all:	plugin
else
ifeq ($(OS), fbsd)
NAME ?= olsrd_dot_draw.so.0.3
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib
MYFLAGS ?= -Wall -fPIC -g # Uncomment -g for debugging
LIBS ?= -lc -lm
CFLAGS ?= -g -O2 -Wall -Wmissing-prototypes -Wstrict-prototypes
INSTALL_LIB = 	install -m 755 $(NAME) $(LIBDIR)/$(NAME);\
	/sbin/ldconfig
all:	plugin
else
all: 	help
endif
endif

CFLAGS += $(PLUGINFLAGS)
#Sourcefiles
SRCS=	src/olsrd_plugin.c src/olsrd_dot_draw.c

#Objectfiles
OBJS=	src/olsrd_plugin.o src/olsrd_dot_draw.o

#Headerfiles
HDRS=	src/olsrd_plugin.h src/olsrd_dot_draw.h src/olsr_plugin_io.h

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
	@echo '****** olsr.org DOT DRAW Make ******'
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

src/olsrd_dot_draw.o: $(HDRS)
