#!/bin/sh

test -f ${0%/*}/src/cfgparser/oparse.c && {
  cat>&2 <<EOF
This script reformats all source files. Be careful
with doing so. You need a clean source tree, e.g.
reformatting of bison/flex output may not work well.

For these reasons: run "make uberclean" first.
EOF
  exit 1
}
test -x $PWD/${0##*/} || {
  cat>&2 <<EOF
************************************************************
Warning: about to change all files below current working dir
$PWD
************************************************************
Proceeed (y/N)
EOF
  read l
  test "y" = "$l" || exit 1
}

sed -i 's/Andreas T.\{1,6\}nnesen/Andreas Tonnesen/g;s/Andreas Tønnesen/Andreas Tonnesen/g;s/Andreas TÃ¸nmnesen/Andreas Tonnesen/' $(find -type f -not -path "*/.hg*" -not -name ${0##*/})
sed -i 's///g;s/[	 ]\+$//' $(find -name "*.[ch]" -not -path "*/.hg*")
indent -sob -nhnl -nut $(find -name "*.[ch]" -not -path "*/.hg*")
rm $(find -name "*~" -not -path "*/.hg*")
