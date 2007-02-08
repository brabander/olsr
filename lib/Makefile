SUBDIRS := $(shell find -maxdepth 2 -name Makefile -not -path ./Makefile -printf "%h\n")

.PHONY: $(SUBDIRS)

all: $(SUBDIRS)
clean: $(SUBDIRS)
install: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)
