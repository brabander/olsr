# programs
CCACHE ?=       $(shell sh -c "type -path ccache")
CC ?= 		$(CCACHE) gcc
STRIP ?=	strip
BISON ?=	bison
FLEX ?=		flex
TAGCMD ?=	etags

# target directories and names
DESTDIR ?=
SBINDIR ?= $(DESTDIR)/usr/sbin
ETCDIR  ?= $(DESTDIR)/etc
LIBDIR  ?= $(DESTDIR)/usr/lib
DOCDIR  ?= $(DESTDIR)/usr/share/doc
MANDIR  ?= $(DESTDIR)/usr/share/man
CFGFILE ?= $(ETCDIR)/olsrd.conf
EXENAME ?= olsrd

INCLUDES =	-Isrc -I$(TOPDIR)/src 

GCC_VERSION := $(shell v=$$($(CC) -dumpversion); echo $${v//./ })

# add gcc warnings and optimizations if CFLAGS not set
ifndef CFLAGS
CFLAGS +=	-Wall
# gcc-3.3.5 on Debian sarge doesn't know the following warnings
ifneq ("$(GCC_VERSION)","3 3 5")
CFLAGS +=	-Wextra
CFLAGS +=	-Wold-style-definition
CFLAGS +=	-Wdeclaration-after-statement
endif
CFLAGS +=	-Wmissing-prototypes
CFLAGS +=	-Wstrict-prototypes
CFLAGS +=	-Wmissing-declarations
CFLAGS +=	-Wsign-compare
CFLAGS +=	-Waggregate-return
CFLAGS +=	-Wmissing-noreturn
CFLAGS +=	-Wmissing-format-attribute
CFLAGS +=	-Wno-multichar
CFLAGS +=	-Wno-deprecated-declarations
#CFLAGS +=	-Wredundant-decls
CFLAGS +=	-Wnested-externs
#CFLAGS +=	-Wunreachable-code
CFLAGS +=	-Winline
CFLAGS +=	-Wdisabled-optimization
CFLAGS +=	-O2
CFLAGS +=	-g
#CFLAGS +=	-Werror
endif

ifdef OLSRD_PLUGIN
# c and ld flags for libraries (plugins)
CFLAGS +=	-DOLSR_PLUGIN
LDFLAGS +=	-shared -Wl,-soname,$(PLUGIN_NAME)
LDFLAGS +=	-Wl,--version-script=version-script.txt 
else
# c and ld flags for main
LDFLAGS +=	-Wl,-export-dynamic 
endif

# search sources and headers in current dir and in src/
SRCS =		$(wildcard src/*.c)
SRCS +=		$(wildcard *.c)
HDRS =		$(wildcard src/*.h)
HDRS +=		$(wildcard *.h)

all: default_target

# OS detection
ifeq ($(OS),)
OS :=		$(shell sh $(TOPDIR)/make/guess_os.sh)
endif
ifeq ($(OS),UNKNOWN)
all: help
else
# include OS specifics
include $(TOPDIR)/make/Makefile.$(OS)
all: default_target
endif

# one object for each source file
OBJS +=		$(patsubst %.c,%.o,$(SRCS))

# debugging or non-debugging flags
ifdef DEBUG
CFLAGS +=	-DDEBUG
endif
ifdef NODEBUG
CFLAGS +=	-DNODEBUG
endif

# let gcc generate dependency information (*.d)
CFLAGS +=	-MD

# we always need the includes and defines
CFLAGS += $(INCLUDES) $(DEFINES)

TAGFILE ?=	src/TAGS

help:
	@echo 
	@echo '***** olsr.org olsr daemon Make ****'
	@echo ' Automatic detection of your OS     '
	@echo ' failed!                            '
	@echo ' You can provide a valid target OS  '
	@echo ' by setting the OS variable! Valid  '
	@echo ' target OSes are:                   '
	@echo ' ---------------------------------  '
	@echo ' linux - GNU/Linux                  '
	@echo ' win32 - MS Windows                 '
	@echo ' fbsd  - FreeBSD                    '
	@echo ' nbsd  - NetBSD                     '
	@echo ' obsd  - OpenBSD                    '
	@echo ' osx   - Mac OS X                   '
	@echo ' ---------------------------------  '
	@echo ' Example - build for windows:       '
	@echo ' make OS=win32                      '
	@echo ' If you are developing olsrd code,  '
	@echo ' exporting the OS variable might    '
	@echo ' be a good idea :-) Have fun!       '
	@echo '************************************'
	@echo

# include dependencies
-include $(SRCS:%.c=%.d)

# Local Variables:
# mode: makefile
# End:
