CC ?= gcc

LDNAME ?= olsrd_cfgparser.so.0.1

OBJS  = olsrd_conf.o oparse.o oscan.o

ifdef MAKELIB
NAME ?= $(LDNAME)
LIBDIR ?= $(INSTALL_PREFIX)/usr/lib
LDFLAGS ?= -shared -Wl,-soname,$(NAME)
CFLAGS ?= -Wall -fPIC -g -DMAKELIB -I..
LIBS  ?= -lm -lc
DEP = libclean
else 
NAME ?= olsrd_cfgparser
LDFLAGS ?=
CFLAGS ?= -Wall -g -DMAKEBIN -I..
endif

BISON ?= bison
FLEX ?= flex


all:	$(NAME)

$(NAME):	$(DEP) $(OBJS)
		$(CC) $(LDFLAGS) -o $(NAME) $(OBJS) $(LIBS)

oscan.c:	oscan.lex oparse.h olsrd_conf.h
		$(FLEX) -ooscan.c oscan.lex

oparse.h:	oparse.c

oparse.c:	oparse.y olsrd_conf.h
		$(BISON) -d -ooparse.c oparse.y

install:
	install -D -m 755 $(NAME) $(LIBDIR)/$(NAME)
	/sbin/ldconfig -n $(LIBDIR)

clean:
		rm -f *.o
		rm -f $(NAME)
		rm -f $(LDNAME)


libclean:
		rm -f *.o



