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
# to the project. For more information see the website or contact
# the copyright holders.
#
# $Id: Makefile,v 1.30 2004/11/30 17:05:05 tlopatic Exp $

VERS =		0.4.8

#OS =		linux
#OS =		fbsd
#OS =		win32
#OS =		osx

CC ?= 		gcc
STRIP ?=	strip
BISON ?=	bison
FLEX ?=		flex

INSTALL_PREFIX ?=

DEFINES = 	-DUSE_LINK_QUALITY
INCLUDES =	-Isrc

DEPFILE =	.depend

SRCS =		$(wildcard src/*.c)
HDRS =		$(wildcard src/*.h)

CFGDIR =	src/cfgparser
CFGOBJS = 	$(CFGDIR)/oscan.o $(CFGDIR)/oparse.o $(CFGDIR)/olsrd_conf.o

ifndef OS
all:		help
else
all:		olsrd
endif

ifeq ($(OS), linux)

SRCS += 	$(wildcard src/linux/*.c) $(wildcard src/unix/*.c)
HDRS +=		$(wildcard src/linux/*.h) $(wildcard src/unix/*.h)
DEFINES += 	-Dlinux
CFLAGS ?=	-Wall -Wmissing-prototypes -Wstrict-prototypes \
		-O2 -g #-pg -DDEBUG #-march=i686
LIBS =		-lm -ldl
MAKEDEPEND = 	makedepend -f $(DEPFILE) -Y $(INCLUDES) $(DEFINES) $(SRCS) >/dev/null 2>&1

else
ifeq ($(OS), fbsd)

SRCS +=		$(wildcard src/bsd/*.c) $(wildcard src/unix/*.c)
HDRS +=		$(wildcard src/bsd/*.h) $(wildcard src/unix/*.h)
DEFINES +=	-D__FreeBSD__
CFLAGS ?=	-Wall -Wmissing-prototypes -Wstrict-prototypes -O2 -g
LIBS =		-lm
MAKEDEPEND = 	makedepend -f $(DEPFILE) $(INCLUDES) $(DEFINES) $(SRCS)

else
ifeq ($(OS), osx)

SRCS +=		$(wildcard src/bsd/*.c) $(wildcard src/unix/*.c)
HDRS +=		$(wildcard src/bsd/*.h) $(wildcard src/unix/*.h)
DEFINES +=	-D__MacOSX__
CFLAGS ?=	-Wall -Wmissing-prototypes -Wstrict-prototypes -O2 -g 
LIBS =		-lm -ldl
MAKEDEPEND = 	makedepend -f $(DEPFILE) $(INCLUDES) $(DEFINES) $(SRCS)

else
ifeq ($(OS), win32)

SRCS +=		$(wildcard src/win32/*.c)
HDRS +=		$(wildcard src/win32/*.h)
INCLUDES += 	-Isrc/win32
DEFINES +=	-DWIN32
CFLAGS ?=	-Wall -Wmissing-prototypes -Wstrict-prototypes \
		-mno-cygwin -O2 -g
LIBS =		-mno-cygwin -lws2_32 -liphlpapi
MAKEDEPEND = 	makedepend -f $(DEPFILE) $(INCLUDES) $(DEFINES) $(SRCS) >/dev/null 2>&1

olsr-${VERS}.zip:	gui/win32/Main/Release/Switch.exe \
		gui/win32/Shim/Release/Shim.exe \
		olsrd.exe \
		src/cfgparser/olsrd_cfgparser.dll \
		README-WIN32.txt \
		gui/win32/Inst/linux-manual.txt \
		files/olsrd.conf.default.win32 \
		gui/win32/Main/Default.olsr \
		lib/dot_draw/olsrd_dot_draw.dll
		rm -rf ${TEMP}/olsr-${VERS}
		rm -f ${TEMP}/olsr-${VERS}.zip
		rm -f olsr-${VERS}.zip
		mkdir ${TEMP}/olsr-${VERS}
		cp gui/win32/Main/Release/Switch.exe ${TEMP}/olsr-${VERS}
		cp gui/win32/Shim/Release/Shim.exe ${TEMP}/olsr-${VERS}
		cp olsrd.exe ${TEMP}/olsr-${VERS}
		cp src/cfgparser/olsrd_cfgparser.dll ${TEMP}/olsr-${VERS}
		cp README-WIN32.txt ${TEMP}/olsr-${VERS}
		cp gui/win32/Inst/linux-manual.txt ${TEMP}/olsr-${VERS}
		cp files/olsrd.conf.default.win32 ${TEMP}/olsr-${VERS}/olsrd.conf
		cp gui/win32/Main/Default.olsr ${TEMP}/olsr-${VERS}
		cp lib/dot_draw/olsrd_dot_draw.dll ${TEMP}/olsr-${VERS}
		cd ${TEMP}; echo y | cacls olsr-${VERS} /T /G Everyone:F
		cd ${TEMP}; zip -q -r olsr-${VERS}.zip olsr-${VERS}
		cp ${TEMP}/olsr-${VERS}.zip .
		rm -rf ${TEMP}/olsr-${VERS}
		rm -f ${TEMP}/olsr-${VERS}.zip

olsr-${VERS}-setup.exe:	gui/win32/Main/Release/Switch.exe \
		gui/win32/Shim/Release/Shim.exe \
		olsrd.exe \
		src/cfgparser/olsrd_cfgparser.dll \
		README-WIN32.txt \
		gui/win32/Inst/linux-manual.txt \
		files/olsrd.conf.default.win32 \
		gui/win32/Main/Default.olsr \
		lib/dot_draw/olsrd_dot_draw.dll \
		gui/win32/Inst/installer.nsi
		rm -f olsr-setup.exe
		rm -f olsr-${VERS}-setup.exe
		C:/Program\ Files/NSIS/makensis gui\win32\Inst\installer.nsi
		mv olsr-setup.exe olsr-${VERS}-setup.exe

endif
endif
endif
endif

OBJS = $(patsubst %.c,%.o,$(SRCS))
override CFLAGS += $(INCLUDES) $(DEFINES)
export CFLAGS


olsrd:		$(OBJS) $(CFGOBJS)
		$(CC) -o $@ $(OBJS) $(CFGOBJS) $(LIBS) 

$(DEPFILE):	$(SRCS) $(HDRS)
ifdef MAKEDEPEND
		@echo '# olsrd dependency file. AUTOGENERATED' > $(DEPFILE)
		$(MAKEDEPEND)
endif

$(CFGOBJS):
		$(MAKE) -C src/cfgparser


.PHONY: help libs clean_libs clean uberclean install_libs install_bin install

help:
	@echo
	@echo '***** olsr.org olsr daemon Make ****'
	@echo ' You must provide a valid target OS '
	@echo ' by setting the OS variable! Valid  '
	@echo ' target OSes are:                   '
	@echo ' ---------------------------------  '
	@echo ' linux - GNU/Linux                  '
	@echo ' win32 - MS Windows                 '
	@echo ' fbsd  - FreeBSD                    '
	@echo ' osx   - Mac OS X                   '
	@echo ' ---------------------------------  '
	@echo ' Example - build for windows:       '
	@echo ' make OS=win32                      '
	@echo ' If you are developing olsrd code,  '
	@echo ' exporting the OS variable might    '
	@echo ' be a good idea :-) Have fun!       '
	@echo '************************************'
	@echo

clean:
		rm -f $(OBJS) olsrd olsrd.exe
		$(MAKE) -C src/cfgparser clean

uberclean:	clean clean_libs
		rm -f $(DEPFILE) $(DEPFILE).bak
		rm -f src/*[o~] src/linux/*[o~] src/unix/*[o~] src/win32/*[o~]
		rm -f src/bsd/*[o~] 
		$(MAKE) -C src/cfgparser uberclean

install_bin:
		$(STRIP) olsrd
		mkdir -p $(INSTALL_PREFIX)/usr/sbin
		install -m 755 olsrd $(INSTALL_PREFIX)/usr/sbin

install:	install_bin
		@echo olsrd uses the configfile $(INSTALL_PREFIX)/etc/olsr.conf
		@echo a default configfile. A sample configfile
		@echo can be installed
		mkdir -p $(INSTALL_PREFIX)/etc
		cp -i files/olsrd.conf.default $(INSTALL_PREFIX)/etc/olsrd.conf
		@echo -------------------------------------------
		@echo Edit $(INSTALL_PREFIX)/etc/olsrd.conf before running olsrd!!
		@echo -------------------------------------------
		mkdir -p $(INSTALL_PREFIX)/usr/share/man/man8/
		cp files/olsrd.8.gz $(INSTALL_PREFIX)/usr/share/man/man8/olsrd.8.gz

libs: 
		for i in lib/*; do \
			$(MAKE) -C $$i; \
		done; 

clean_libs: 
		for i in lib/*; do \
			$(MAKE) -C $$i clean; \
		done; 

install_libs:
		for i in lib/*; do \
			$(MAKE) -C $$i LIBDIR=$(INSTALL_PREFIX)/usr/lib install; \
		done; 	

sinclude	$(DEPFILE)
