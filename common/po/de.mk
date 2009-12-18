.PHONY: common-de install-common-de clean-common-de uninstall-common-de

COMMON_PO_MAKE      += common-de
COMMON_PO_INSTALL   += common-install-de
COMMON_PO_CLEAN     += common-clean-de
COMMON_PO_UNINSTALL += common-uninstall-de

common-de:
	 @msgfmt po/de.po -o de.mo
	-@echo "generated \`po/de.po' --> \`de.mo'"

common-clean-de:
	-@/bin/rm -fv de.mo
