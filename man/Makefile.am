dist_man_MANS = ziproxy.1 ziproxylogtool.1

MANPAGE_LOCALES = 

EXTRA_DIST = $(wildcard ziproxy.??.1) $(wildcard ziproxylogtool.??.1)

install-data-local:
	if [ ! -d $(DESTDIR)$(STATEDIR) ]; then \
		$(mkinstalldirs) $(DESTDIR)$(STATEDIR); \
	fi

	for loc in $(MANPAGE_LOCALES); do \
		$(mkinstalldirs) $(DESTDIR)$(mandir)/$$loc/man1; \
		$(INSTALL_DATA) $(srcdir)/ziproxy.$$loc.1 $(DESTDIR)$(mandir)/$$loc/man1/ziproxy.1; \
		$(INSTALL_DATA) $(srcdir)/ziproxylogtool.$$loc.1 $(DESTDIR)$(mandir)/$$loc/man1/ziproxylogtool.1; \
	done

uninstall-local:
	for loc in $(MANPAGE_LOCALES); do \
		rm -f $(DESTDIR)$(mandir)/$$loc/man1/ziproxy.1; \
		rm -f $(DESTDIR)$(mandir)/$$loc/man1/ziproxylogtool.1; \
	done

