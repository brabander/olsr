MANS += olsrd.8.gz
MANS += olsrd.conf.5.gz
MANS += olsrd-metrics.3

# Requires 'apt-get install docbook2x docbook-xml'
$(MANS): olsrd-manpages.xml
	docbook2x-man --solinks $^
	gzip -f $(patsubst %.gz,%, $(MANS))

# Requires 'apt-get install docbook-utils'
index.html: olsrd-manpages.xml
	docbook2html -V $$(echo "(define\r%html-header-tags%\r'((\"META\"(\"HTTP-EQUIV\"\"Content-Type\")(\"CONTENT\"\"text/html;charset=utf-8\"))))") -V '%use-id-as-filename%'  $^

clean:
	rm -fv $(MANS)
	rm -fv $(patsubst %.gz,%, $(MANS))
	rm -fv index.html
	rm -fv $(patsubst %_gz,%.html, $(subst .,_,$(MANS)))
