#!/bin/sh

srcs="\
    src/build_msg.c \
    src/configfile.c \
    src/duplicate_set.c \
    src/generate_msg.c \
    src/hashing.c \
    src/hna_set.c \
    src/hysteresis.c \
    src/interfaces.c \
    src/ipc_frontend.c \
    src/link_set.c \
    src/local_hna_set.c \
    src/main.c \
    src/mantissa.c \
    src/mid_set.c \
    src/mpr.c \
    src/mpr_selector_set.c \
    src/neighbor_table.c \
    src/net.c \
    src/olsr.c \
    src/packet.c \
    src/parser.c \
    src/plugin.c \
    src/plugin_loader.c \
    src/process_package.c \
    src/process_routes.c \
    src/rebuild_packet.c \
    src/routing_table.c \
    src/scheduler.c \
    src/socket_parser.c \
    src/tc_set.c \
    src/two_hop_neighbor_table.c \
    src/win32/apm.c \
    src/win32/compat.c \
    src/win32/ifnet.c \
    src/win32/kernel_routes.c \
    src/win32/log.c \
    src/win32/net.c \
    src/win32/tunnel.c \
    "

rm -f Makefile.win32
cp Makefile.win32.in Makefile.win32

echo >>Makefile.win32
echo \# >>Makefile.win32
echo \# DEPENDENCIES >>Makefile.win32
echo \# >>Makefile.win32

for f in $srcs; do
    echo >>Makefile.win32
    echo \# $f >>Makefile.win32
    echo >>Makefile.win32

    o=`echo $f | sed -e 's/c$/o/'`

    gcc -MM -MT $o -mno-cygwin -O2 -Wall -c -DWIN32 -DDEBUG -Isrc -Isrc/win32 $f >>Makefile.win32
done
