# OLSRd Quagga plugin
#
# Copyright (C) 2006-2008 Immo 'FaUl' Wehrenberg <immo@chaostreff-dortmund.de>
# Copyright (C) 2007-2008 Vasilis Tsiligiannis <acinonyxs@yahoo.gr>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation or - at your option - under
# the terms of the GNU General Public Licence version 2 but can be
# linked to any BSD-Licenced Software with public available sourcecode
#

OLSRD_PLUGIN =	true
PLUGIN_NAME =	olsrd_quagga
PLUGIN_VER =	0.2.2

TOPDIR = ../..
include $(TOPDIR)/Makefile.inc

CFLAGS += -g
CPPFLAGS +=-DUSE_UNIX_DOMAIN_SOCKET

ifeq ($(OS),win32)

default_target install clean:
	@echo "*** Quagga not supported on Windows (so it would be pointless to build the Quagga plugin)"

else

default_target: $(PLUGIN_FULLNAME)

$(PLUGIN_FULLNAME): $(OBJS) version-script.txt
		$(CC) $(LDFLAGS) -o $(PLUGIN_FULLNAME) $(OBJS) $(LIBS)

install:	$(PLUGIN_FULLNAME)
		$(STRIP) $(PLUGIN_FULLNAME)
		$(INSTALL_LIB)

clean:
		rm -f $(OBJS) $(SRCS:%.c=%.d) $(PLUGIN_FULLNAME)

endif
