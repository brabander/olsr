
# The olsr.org Optimized Link-State Routing daemon(olsrd)
# Copyright (c) 2004, Andreas Tønnesen(andreto@olsr.org)
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions 
# are met:
#
# * Redistributions of source code must retain the above copyright 
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright 
#   notice, this list of conditions and the following disclaimer in 
#   the documentation and/or other materials provided with the 
#   distribution.
# * Neither the name of olsr.org, olsrd nor the names of its 
#   contributors may be used to endorse or promote products derived 
#   from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
# POSSIBILITY OF SUCH DAMAGE.
#
# Visit http://www.olsr.org for more information.
#
# If you find this software useful feel free to make a donation
# to the projcet. For more information see the website or contact
# the copyright holders.
#
# $Id: Makefile,v 1.9 2004/11/21 17:10:27 tlopatic Exp $




#OS =		linux
#OS =		fbsd
#OS =		win32
#OS =		osx

ifeq ($(OS), win32)

LIBNAME ?=	olsrd_cfgparser.dll
BINNAME ?=	olsrd_cfgparser.exe

PORT_CFLAGS =	-mno-cygwin -I../win32 -DWIN32_STDIO_HACK
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
