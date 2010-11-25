.PHONY: clean

binary = de.mo

all:$(binary)

%.mo: %.po
	@msgfmt -o ../$@ $<
	@echo "translated \`$<' --> \`$@'"

clean:
	@rm -fv ../$(binary)

distclean: clean
