#!/bin/sh

srcs="\
    src/olsrd_plugin.c \
    src/olsrd_dot_draw.c \
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

    gcc -MM -MT $o -mno-cygwin -O2 -Wall -c -DWIN32 -I../../src/win32 $f >>Makefile.win32
done
