SUBDIRS := $(shell find -maxdepth 1 -type d -not -name ".*" -not -name "CVS" -printf "%f\n")

.PHONY: $(SUBDIRS)

all: $(SUBDIRS)
clean: $(SUBDIRS)
install: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)
