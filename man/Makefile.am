## Process this file with automake to produce Makefile.in

man_MANS = newscache.conf.5\
	newscache.8\
	newscacheclean.8\
	updatenews.8

EXTRA_DIST = newscache.conf.5.in\
	newscache.8.in\
	newscacheclean.8.in\
	updatenews.8.in

CLEANFILES = $(man_MANS)

SUFFIXES = .in

.in:

	@RM@ -f $@ $@-temp
	@SED@ \
		-e 's!@SYSCONFDIR@!$(sysconfdir)!g' \
		-e 's!@SBINDIR@!$(sbindir)!g' \
		$< > $@-temp
	@MV@  $@-temp $@

