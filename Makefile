CC ?= gcc

CFLAGS ?= -Wall -O2 #-g #-pg -DDEBUG #-march=i686
LIBS = -lpthread -lm -ldl
INSTALL_PREFIX ?=


# Keep OS specific files last

SRCS=	src/interfaces.c src/parser.c src/build_msg.c \
	src/scheduler.c src/main.c src/two_hop_neighbor_table.c \
	src/neighbor_table.c src/mpr_selector_set.c src/duplicate_set.c \
	src/tc_set.c src/routing_table.c src/packet.c src/olsr.c \
	src/process_routes.c src/net.c src/mantissa.c \
	src/hna_set.c src/mid_set.c src/ipc_frontend.c \
	src/link_set.c src/configfile.c src/socket_parser.c \
	src/process_package.c src/mpr.c src/local_hna_set.c \
	src/hashing.c src/hysteresis.c src/generate_msg.c \
	src/rebuild_packet.c src/plugin_loader.c src/plugin.c \
	src/linux/net.c src/linux/apm.c src/linux/tunnel.c \
	src/linux/kernel_routes.c src/linux/link_layer.c \
	src/linux/ifnet.c src/linux/log.c

OBJS=	src/interfaces.o src/parser.o src/build_msg.o \
	src/scheduler.o src/main.o src/two_hop_neighbor_table.o \
	src/neighbor_table.o src/mpr_selector_set.o src/duplicate_set.o \
	src/tc_set.o src/routing_table.o src/packet.o src/olsr.o \
	src/process_routes.o src/net.o src/mantissa.o \
	src/hna_set.o src/mid_set.o src/ipc_frontend.o \
	src/link_set.o src/configfile.o src/socket_parser.o \
	src/process_package.o src/mpr.o src/local_hna_set.o\
	src/hashing.o src/hysteresis.o src/generate_msg.o \
	src/rebuild_packet.o src/plugin_loader.o src/plugin.o \
	src/linux/net.o src/linux/apm.o src/linux/tunnel.o \
	src/linux/kernel_routes.o src/linux/link_layer.o \
	src/linux/ifnet.o src/linux/log.o

HDRS=	src/defs.h src/interfaces.h src/packet.h src/build_msg.h \
	src/olsr.h src/two_hop_neighbor_table.h olsr_plugin_io.h \
	src/neighbor_table.h src/mpr_selector_set.h \
	src/duplicate_set.h src/tc_set.h src/rtable.h \
	src/process_routes.h src/net.h src/mantissa.h \
	src/hna_set.h main.h src/mid_set.h src/ipc_frontend.h \
	src/olsr_protocol.h src/link_set.h src/configfile.h \
	src/process_package.h src/mpr.h src/ipc_olsrset.h \
	src/local_hna_set.h src/hashing.h src/hysteresis.h \
	src/generate_msg.h src/rebuild_packet.h src/plugin_loader.h \
	src/plugin.h src/socket_parser.h src/ifnet.h \
	src/kernel_routes.h src/log.h src/net_os.h \
	src/apm.h src/linux/tunnel.h src/scheduler.h \
	src/linux/net.h	src/linux/link_layer.h

all:	olsrd

olsrd:	$(OBJS)
	$(CC) $(LIBS) -o bin/$@ $(OBJS)

libs: 
	for i in lib/*; do \
		$(MAKE) -C $$i; \
	done; 

clean_libs: 
	for i in lib/*; do \
		$(MAKE) -C $$i clean; \
	done; 

.PHONY: clean
clean:
	-rm -f $(OBJS)

install_libs:
	for i in lib/*; do \
		$(MAKE) -C $$i LIBDIR=$(INSTALL_PREFIX)/usr/lib install; \
	done; 	


install_bin:
	install -D -m 755 bin/olsrd $(INSTALL_PREFIX)/usr/sbin/olsrd

install: install_bin
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
