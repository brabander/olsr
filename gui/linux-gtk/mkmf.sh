#!/bin/sh

srcs="\
    src/main.c \
    src/interface.c \
    src/callbacks.c \
    src/ipc.c \
    src/packet.c \
    src/nodes.c \
    src/routes.c \
    src/win32/compat.c \
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

    gcc -MM -MT $o -mno-cygwin -mms-bitfields -O2 -Wall -c -DWIN32 -Isrc/win32 $f >>Makefile.win32
done
