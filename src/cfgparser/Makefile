#
# OLSR ad-hoc routing table management protocol
# Copyright (C) 2003-2004 Andreas Tønnesen (andreto@ifi.uio.no)
#
# This file is part of the olsr.org OLSR daemon.
#
# olsr.org is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# olsr.org is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with olsr.org; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#
# $Id: Makefile,v 1.7 2004/11/09 00:42:47 tlopatic Exp $
#

#OS =		linux
#OS =		fbsd
#OS =		win32
#OS =		osx

ifeq ($(OS), win32)

LIBNAME ?=	olsrd_cfgparser.dll
BINNAME ?=	olsrd_cfgparser.exe

PORT_CFLAGS =	-mno-cygwin -I../win32
PORT_LDFLAGS =	-mno-cygwin
PORT_OBJS =	../win32/compat.o
PORT_LIBS =	-lws2_32

else
ifeq ($(OS), linux)

LIBNAME ?=	olsrd_cfgparser.so.0.1
BINNAME ?=	olsrd_cfgparser

PORT_CFLAGS =	-fPIC
PORT_LDFLAGS =	-Wl,--version-script=version-script.txt
PORT_OBJS =
PORT_LIBS =	-lm -lc

else
ifeq ($(OS), fbsd)

LIBNAME ?=	olsrd_cfgparser.so.0.1
BINNAME ?=	olsrd_cfgparser

PORT_CFLAGS =	-fPIC
PORT_LDFLAGS =	-Wl,--version-script=version-script.txt
PORT_OBJS =
PORT_LIBS =	-lm -lc

else
ifeq ($(OS), osx)

LIBNAME ?=	olsrd_cfgparser.so.0.1
BINNAME ?=	olsrd_cfgparser

PORT_CFLAGS =	-fPIC
PORT_LDFLAGS =	-Wl,--version-script=version-script.txt
PORT_OBJS =
PORT_LIBS =	-lm -lc

endif
endif
endif
endif

CC ?=		gcc
OBJS =		olsrd_conf.o oparse.o oscan.o $(PORT_OBJS)

ifdef MAKELIB

LIBDIR ?=	$(PREFIX)/usr/lib
LDFLAGS ?=	-shared -Wl,-soname,$(LIBNAME) $(PORT_LDFLAGS)
CFLAGS ?=	-Wall -Wmissing-prototypes -Wstrict-prototypes \
		-g -DMAKELIB -I.. $(PORT_CFLAGS)
LIBS ?=		$(PORT_LIBS)

NAME =		$(LIBNAME)

else 

LDFLAGS ?=	$(PORT_LDFLAGS)
CFLAGS ?=	-Wall -Wmissing-prototypes -Wstrict-prototypes \
		-g -DMAKEBIN -I.. $(PORT_CFLAGS)
LIBS ?=		$(PORT_LIBS)

NAME =		$(BINNAME)

endif

BISON ?=	bison
FLEX ?=		flex

all:		clean $(NAME)

$(NAME):	$(OBJS)
		$(CC) $(LDFLAGS) -o $(NAME) $(OBJS) $(LIBS)
		rm -f *.o

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
		rm -f $(LIBNAME)
		rm -f $(BINNAME)
