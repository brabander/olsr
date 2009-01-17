#!/bin/sh

# ./contrib/netsimpcap is GPLv3
EXCEPT="$EXCEPT -not -path './contrib/netsimpcap/*'"

# ./gui/linux-gtk/* is GPLv2
EXCEPT="$EXCEPT -not -path './gui/linux-gtk/*'"

# ./gui/win32/Main/StdAfx.cpp/h are generated
EXCEPT="$EXCEPT -not -path './gui/win32/Main/StdAfx.*'"

# ./gui/win32/Main/resource.h is generated
EXCEPT="$EXCEPT -not -path './gui/win32/Main/resource.h'"

# ./lib/bmf is other legal body
EXCEPT="$EXCEPT -not -path './lib/bmf/*'"

# ./lib/quagga states GPLv2 or LGPLv2
EXCEPT="$EXCEPT -not -path './lib/quagga/*'"

# ./lib/secure/src/md5.[ch] have some homegrown license from RSA Inc.
EXCEPT="$EXCEPT -not -path './lib/secure/src/md5.*'"

# ./src/win32/ce/ws2tcpip.h has none
EXCEPT="$EXCEPT -not -path './src/win32/ce/ws2tcpip.h'"

get_license()
{
  echo "$1"|sed '
    s/ *$//
    1d
    /^Copyright/,/^ /{
      r/dev/stdin
      d
    }
    $a\
\
Visit http://www.olsr.org for more information.\
\
If you find this software useful feel free to make a donation\
to the project. For more information see the website or contact\
the copyright holders.
  ' ${0%.*}.txt
}

put_license()
{
  case "$1" in
    "")
      echo "Please provide a file name" >&2 && exit
    ;;
    ./src/common/string.*|./src/ipcalc.*|./src/plugin_util.*)
      get_license "$(hg log $1|sed '/2208:4b42f04361a3/,/^$/d'|sed -n '/user:/{s/[^ ]* *//;s/.$/&/;h};/date:/{s/[^ ]* *[^ ]* *[^ ]* *[^ ]* *[^ ]* */Copyright (c) /;s/ [^ ]*$/, /;G;s/\n//;s/@/æ/;p}'|sort|uniq)"
      ;;
    *)
      get_license "Copyright (c) 2004-$(date +%Y), the olsr.org team - see HISTORY file"
    ;;
  esac
}

put_source()
{
  put_license $1 | sed '
    1i\
/*
    $a\
 *\
 */
    s/./ * &/
    s/^$/ */
  ' | sed -i '
    /\/\*/{
      N
      s/The olsr.org Optimized Link-State Routing daemon/&/
      tzap
      b
    :zap
      N
      s/\*\///
      tend
      bzap
    :end
      r/dev/stdin
      d
    }
  ' $1
}

put_makefile()
{
  put_license $1 | sed '
    s/./# &/
    s/^$/#/
    $a#\
' | sed -i '
    1,/^$/{
      r/dev/stdin
      d
    }
  ' $1
}

put_nsis()
{
  put_license $1 | sed '
    1i;
    s/./;  &/
    s/^$/;/
    $a;\
;\
' | sed -i '
    1,/^$/{
      r/dev/stdin
      d
    }
  ' $1
}

for file in $(eval find -type f -name "*.[ch]" $EXCEPT);do
  put_source $file
done

for file in $(eval find -type f -name "*.cpp" $EXCEPT);do
  put_source $file
done

for file in $(eval find -type f -name "Makefile" $EXCEPT);do
  put_makefile $file
done

for file in $(eval find -type f -name "*.nsi" $EXCEPT);do
  put_nsis $file
done
