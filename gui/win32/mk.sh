#!/bin/sh

#CFLAGS="-O0 -ggdb -DDEBUG"
CFLAGS="-O2 -DNODEBUG -DNDEBUG"

SRCS="olsr_cfg olsr_cfg_gen olsr_ip_acl olsr_ip_prefix_list olsr_logging_data ipcalc builddata common/autobuf"
OBJS=

for i in $SRCS; do
  gcc -Werror -Wall -I../../src $CFLAGS -c ../../src/$i.c
  OBJS="$OBJS ${i##*/}.o"
done
gcc -Werror -Wall -I../../src $CFLAGS -o win32verify win32verify.c $OBJS
