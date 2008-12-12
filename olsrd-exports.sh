#!/bin/sh

# The GNUL linker accepts a '--dynamic-list=[file]' option since
# binutils-2.18 wich can be used instead of --export-dynamic. This
# shrinks the binary because not all exportable symbols are exported.
# To find symbols which are imported from olsrd plugins, we surround
# all those symboles with EXPORT(sym) in the include files. Use the
# outcome of this script as input file, e.g with the following cmd:
# make all LDFLAGS=-Wl,--dynamic-list=$(./olsrd-exports.sh $(find src -name "*.h"))

sed -n -e '1s/.*/{/p;$s/.*/};/p;/#define/d;s/.*\<EXPORT\>[ 	]*(\([^)]\+\)).*/  "\1";/p' $*
