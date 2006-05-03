SUBDIRS = dot_draw dyn_gw httpinfo mini nameservice powerinfo secure tas bmf

.PHONY: $(SUBDIRS)

all: $(SUBDIRS)
clean: $(SUBDIRS)
install: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)
