CC ?= gcc
STRIP ?= strip
NAME = olsrd_nameservice.so.0.1
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib
MYFLAGS = -Wall -fPIC -D_GNU_SOURCE
LIBS ?= -lc -lm
CFLAGS ?= -O2
CFLAGS += $(MYFLAGS)

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))
HDRS = $(wildcard src/*.c)

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
src/nameservice.o: $(HDRS)
