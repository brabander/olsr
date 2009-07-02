######################
#
# Highlevel configuration options for all
#
#

# set DEBUG to 1 to add debugger symbols to olsrd
# set DEBUG to 0 to remove both and optimize olsrd for size
DEBUG ?= 1

# set to 1 to remove a severity level of logging from olsrd
REMOVE_LOG_DEBUG ?= 0
REMOVE_LOG_INFO  ?= 0
REMOVE_LOG_WARN  ?= 0
REMOVE_LOG_ERROR ?= 0

# enable mudflap with 1 or deactivate with 0
# you need a recent enough gcc and the libmudflap installed
MUDFLAP ?= 0

# enable static linking of plugins (list of plugin directory names) 
STATIC_PLUGINS ?=

######################
#
# Lowlevel options and rules
#

# programs
CCACHE ?=       $(shell IFS=:; for i in $$PATH;do test -x "$$i/ccache" && echo "$$i/ccache" && break; done)
ifeq ($(origin CC),default)
  CC = 		$(CCACHE) gcc
else
  CC ?= 		$(CCACHE) gcc
endif
ifeq ($(DEBUG),0)
  STRIP ?=	strip
else
  STRIP ?=	:
endif

BISON ?=	bison
FLEX ?=		flex
TAGCMD ?=	etags
LD_HAS_DYN ?=   $(shell $(CC) -Wl,--help 2>&1|grep -w -q -- --dynamic-list && echo yes)

# target directories and names
PREFIX  ?= /usr/local
DESTDIR ?=
SBINDIR ?= $(DESTDIR)$(PREFIX)/sbin
ETCDIR  ?= $(DESTDIR)$(subst /usr/etc,/etc,$(PREFIX)/etc)
LIBDIR  ?= $(DESTDIR)$(PREFIX)/lib
DOCDIR  ?= $(DESTDIR)$(PREFIX)/share/doc
MANDIR  ?= $(DESTDIR)$(PREFIX)/share/man
EXENAME ?= olsrd
CFGNAME ?= $(EXENAME).conf
CFGFILE ?= $(ETCDIR)/$(CFGNAME)

CPPFLAGS =	$(filter-out -I./src,-Isrc -I$(TOPDIR)/src)
CPPFLAGS +=	-DOLSRD_CONF_FILE_NAME='"$(CFGNAME)"'
CPPFLAGS +=	-DOLSRD_GLOBAL_CONF_FILE='"$(CFGFILE)"'

#############
# handle debugging and logging options
#############
ifeq ($(REMOVE_LOG_DEBUG),1)
  CPPFLAGS +=	-DREMOVE_LOG_DEBUG
endif
ifeq ($(REMOVE_LOG_INFO),1)
  CPPFLAGS +=	-DREMOVE_LOG_INFO
endif
ifeq ($(REMOVE_LOG_WARN),1)
  CPPFLAGS +=	-DREMOVE_LOG_WARN
endif
ifeq ($(REMOVE_LOG_ERROR),1)
  CPPFLAGS +=	-DREMOVE_LOG_ERROR
endif

##############
# add gcc warnings and optimizations if CFLAGS not set
##############
ifndef CFLAGS
  ifndef WARNINGS
    ALL_WARNINGS +=	-Wall
    ALL_WARNINGS +=	-Wextra
    ALL_WARNINGS +=	-Wold-style-definition
    ALL_WARNINGS +=	-Wdeclaration-after-statement
    ALL_WARNINGS +=	-Wmissing-prototypes
    ALL_WARNINGS +=	-Wstrict-prototypes
    ALL_WARNINGS +=	-Wmissing-declarations
    ALL_WARNINGS +=	-Wsign-compare
    ALL_WARNINGS +=	-Waggregate-return
    ALL_WARNINGS +=	-Wmissing-noreturn
    ALL_WARNINGS +=	-Wmissing-format-attribute
    ALL_WARNINGS +=	-Wno-multichar
    ALL_WARNINGS +=	-Wno-deprecated-declarations
    ALL_WARNINGS +=	-Wendif-labels
    ALL_WARNINGS +=	-Wwrite-strings
    ALL_WARNINGS +=	-Wbad-function-cast
    ALL_WARNINGS +=	-Wpointer-arith
    # ALL_WARNINGS +=	-Wcast-qual
    ALL_WARNINGS +=	-Wshadow
    ALL_WARNINGS +=	-Wformat-2
    ALL_WARNINGS +=	-Wsequence-point
    ALL_WARNINGS +=	-Wpointer-arith
    ALL_WARNINGS +=	-Wcast-align

    # the following 2 do not work yet and need more work on it
    #ALL_WARNINGS +=	-Wconversion
    #ALL_WARNINGS +=	-Wredundant-decls

    ifeq ($(MUDFLAP),0)
      # work around a bug in gcc-4.*
      ALL_WARNINGS +=	-Wnested-externs
    endif

    # Alas, htons() triggers this so we can't seriously activate it.
    #ALL_WARNINGS +=	-Wunreachable-code

    ALL_WARNINGS +=	-Winline
    ALL_WARNINGS +=	-Wdisabled-optimization
    ALL_WARNINGS +=	-Werror
    ALL_WARNINGS +=	-finline-functions-called-once
    ALL_WARNINGS +=	-funit-at-a-time
    ALL_WARNINGS +=	-fearly-inlining

    ifeq ($(DEBUG),0)
      ALL_WARNINGS +=	-fomit-frame-pointer
    endif

    # we have small inline functions in src/ipcalc.h which should always be inlined
    ALL_WARNINGS +=	-finline-limit=350

    # These tell gcc to put each function and global variable in a separate section.
    # The linker can than remove all unreferenced section. But in the olsrd binary
    # unused doesn't imply unused at all since the function may be used by plugins,
    # e.g. the ones in src/plugin_utils.c.
    # So we can use that feature at most to identify unused functions and remove them
    # from the source by hand.

    #ALL_WARNINGS +=	-ffunction-sections
    #ALL_WARNINGS +=	-fdata-sections
    ALL_WARNINGS +=	-Wstrict-overflow=5
    ALL_WARNINGS +=	$(EXTRA_WARNINGS)
    WARNINGS :=	$(shell CC="$(CC)" $(TOPDIR)/gcc-warnings $(ALL_WARNINGS) 2> /dev/null)
  endif

  CFLAGS +=	$(WARNINGS)
  CFLAGS +=	$(OPTIMIZE)
  CFLAGS +=	$(EXTRA_CFLAGS)

  ifneq ($(filter -Wstrict-overflow%,$(WARNINGS)),)
    NO_STRICT_OVERFLOW = -Wstrict-overflow=0
  endif

endif # ifndef CFLAGS

ifeq ($(DEBUG),1)
  CPPFLAGS +=	-DDEBUG
  CFLAGS +=	-ggdb
  OPTIMIZE ?=	-O0
else
  OPTIMIZE ?=	-Os
endif

ifneq ($(MUDFLAP),0)
  CFLAGS +=	-fmudflapth
endif

ifdef OLSRD_PLUGIN
  # c and ld flags for libraries (plugins)
  CPPFLAGS +=	-DOLSR_PLUGIN
  LDFLAGS +=	-shared
  LDFLAGS +=	-Wl,-soname,$(PLUGIN_NAME)
  LDFLAGS +=	-Wl,--version-script=version-script.txt
else
  # c and ld flags for main
  ifeq ($(LD_HAS_DYN), yes)
    LDFLAGS_EXE +=	-Wl,--dynamic-list=$(EXENAME).exports
  else
    LDFLAGS_EXE +=	-Wl,-export-dynamic
  endif
  LDFLAGS +=	-Wl,-rpath,$(LIBDIR)
endif

# LDFLAGS for all
LDFLAGS +=	-Wl,--warn-common

# See above at "-ffunction-sections" for an explanation (and why it is disabled).
#LDOPTS +=	-Wl,--gc-sections
#LDOPTS +=	-Wl,--print-gc-sections
#LDFLAGS +=	$(shell CC="$(CC)" $(TOPDIR)/ld-warnings $(LDOPTS))
ifneq ($(MUDFLAP),0)
  LIBS +=		-lmudflapth
endif

# extra options from the outside
CPPFLAGS +=	$(EXTRA_CPPFLAGS)
LDFLAGS +=	$(EXTRA_LDFLAGS)

# search sources and headers in current dir and in src/
SRCS +=		$(wildcard src/common/*.c src/*.c)
HDRS +=		$(wildcard src/common/*.h src/*.h)

#get headers and object files for static plugins
STATIC_PLUGIN_SRCS += $(foreach dir,$(STATIC_PLUGINS),$(wildcard lib/$(dir)/src/*.c))
STATIC_PLUGIN_OBJS += $(STATIC_PLUGIN_SRCS:%.c=%.o)

# OS detection
ifeq ($(OS),Windows_NT)
  OS := win32
endif
ifeq ($(OS),)
  OS := $(shell sh $(TOPDIR)/make/guess_os.sh)
endif
ifeq ($(OS),UNKNOWN)
all: help
else
# include OS specifics
all: default_target
include $(TOPDIR)/make/Makefile.$(OS)
endif

ifdef OLSRD_PLUGIN
  # this must be run after the OS-specific makefile
  CPPFLAGS +=  -DPLUGIN_FULLNAME="\"$(PLUGIN_FULLNAME)\""
endif

# one object for each source file
OBJS +=		$(SRCS:%.c=%.o)

# a make function to quote "/" and "."
quote = $(subst .,\.,$(subst /,\/,$1))

# fully automatic and working dependency generation
%.d: %.c
	@$(filter-out $(CCACHE),$(CC)) -M $(strip $(CPPFLAGS)) "$<" | sed -e '1s/\($(call quote,$(*F))\.o\)[ :]*/$(call quote,$(*D)/\1 $@: Makefile $(if $(TOPDIR),$(TOPDIR)/)Makefile.inc) /g' >"$@"

src/common/autobuf.o src/mpr.o: CFLAGS += $(NO_STRICT_OVERFLOW)

# we always need the includes and defines
# for legacy since now
CPPFLAGS += $(INCLUDES) $(DEFINES)
ifneq ($(INCLUDES),)
  $(warning Use CPPFLAGS instead of INCLUDES for -I)
endif
ifneq ($(DEFINES),)
  $(warning Use CPPFLAGS instead of DEFINES for -D)
endif

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

ifeq ($(filter clean% %clean rpm, $(MAKECMDGOALS)),)
# include dependencies - we don't need any dependency for a everytime generated files
-include $(filter-out src/builddata.%,$(SRCS:%.c=%.d))
endif

# Local Variables:
# mode: makefile
# End:
