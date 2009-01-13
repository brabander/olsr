#!/bin/sh

SRCS="olsr_cfg olsr_cfg_gen olsr_ip_acl olsr_ip_prefix_list ipcalc builddata common/autobuf"
OBJS=

for i in $SRCS; do
  gcc -Werror -Wall -I../../src -O0 -ggdb -c ../../src/$i.c
  OBJS="$OBJS ${i##*/}.o"
done
gcc -Werror -Wall -I../../src -O0 -ggdb -o win32verify win32verify.c $OBJS
