MANS += olsrd.8.gz
MANS += olsrd.conf.5.gz
MANS += olsr_switch.8.gz

$(MANS): olsrd-manpages.xml
	docbook2x-man --solinks $^
	gzip -f $(patsubst %.gz,%, $(MANS))

index.html: olsrd-manpages.xml
	docbook2html -V '%use-id-as-filename%' $^

clean:
	rm -fv $(MANS)
	rm -fv $(patsubst %.gz,%, $(MANS))
	rm -fv index.html
	rm -fv $(patsubst %_gz,%.html, $(subst .,_,$(MANS)))
