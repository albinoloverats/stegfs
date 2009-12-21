.PHONY: de install-de clean-de uninstall-de

PO_MAKE      += de
PO_INSTALL   += install-de
PO_CLEAN     += clean-de
PO_UNINSTALL += uninstall-de

de:
	 @msgfmt po/de.po common/po/de.po -o de.mo
	-@echo "generated \`po/de.po' --> \`de.mo'"

install-de:
	 @install -c -m 644 -D -T de.mo $(PREFIX)/usr/share/locale/de/LC_MESSAGES/stegfs.mo
	-@echo "installed \`de.mo' --> \`$(PREFIX)/usr/share/locale/de/LC_MESSAGES/stegfs.mo'"

clean-de:
	-@/bin/rm -fv de.mo

uninstall-de:
	-@/bin/rm -fv $(PREFIX)/usr/share/locale/de/LC_MESSAGES/stegfs.mo