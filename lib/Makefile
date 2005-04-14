
REALOS = $(OS)

ifeq ($(OS), linux)

all:		subdirs
install:        subdirs
clean:          subdirs
SUBDIRS = dot_draw dyn_gw httpinfo nameservice powerinfo secure tas

else
ifeq ($(OS), win32)

all:		subdirs
install:        subdirs
clean:          subdirs
SUBDIRS = dot_draw dyn_gw httpinfo nameservice powerinfo secure tas

else
ifeq ($(OS), fbsd)

all:		subdirs
install:        subdirs
clean:          subdirs
SUBDIRS = dot_draw dyn_gw httpinfo nameservice powerinfo secure

else
ifeq ($(OS), fbsd-ll)

REALOS = fbsd
all:		subdirs
install:	subdirs
clean:          subdirs
SUBDIRS = dot_draw dyn_gw httpinfo nameservice powerinfo secure

else

all: 		help
install::	help
clean:          help

endif
endif
endif
endif

.PHONY:	subdirs $(SUBDIRS)

subdirs:	$(SUBDIRS)

$(SUBDIRS):
	@echo $(MAKE) -C $@ OS=$(REALOS) $(MAKECMDGOALS)
	$(MAKE) -C $@ OS=$(REALOS) $(MAKECMDGOALS)

help:
	@echo
	@echo '******** olsr.org plugin Make *******'
	@echo ' You must provide a valid target OS '
	@echo ' by setting the OS variable! Valid  '
	@echo ' target OSes are:                   '
	@echo ' ---------------------------------  '
	@echo ' linux - GNU/Linux                  '
	@echo ' win32 - MS Windows                 '
	@echo ' fbsd  - FreeBSD                    '
	@echo ' ---------------------------------  '
	@echo ' Example - build for windows:       '
	@echo ' make OS=win32                      '
	@echo '************************************'
	@echo

