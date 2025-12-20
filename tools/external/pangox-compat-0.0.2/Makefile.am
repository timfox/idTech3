MAINTAINERCLEANFILES = \
	$(srcdir)/INSTALL \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/autoscan.log \
	$(srcdir)/compile \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.sub \
	$(srcdir)/configure.scan \
	$(srcdir)/depcomp \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/missing \
	$(srcdir)/mkinstalldirs \
	`find "$(srcdir)" -type f -name Makefile.in -print`

EXTRA_DIST = autogen.sh pangox.def pangox.aliases


pangoincludedir=$(includedir)/pango-1.0/pango
pangoinclude_HEADERS = pangox.h

aliasdir = $(sysconfdir)/pango
alias_DATA = pangox.aliases

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = pangox.pc


lib_LTLIBRARIES = libpangox-1.0.la

libpangox_1_0_la_LDFLAGS = -version-info 0:0:0 -export-symbols-regex "^pango_x_"
libpangox_1_0_la_CPPFLAGS = 			\
	-DG_LOG_DOMAIN=\"PangoX-Compat\"	\
	-DPANGO_ENABLE_BACKEND			\
	-DPANGO_ENABLE_ENGINE			\
	-DPANGO_MODULE_PREFIX=_pango_basic_x	\
	-DSYSCONFDIR=\"$(sysconfdir)\"		\
	$(PANGO_CFLAGS)				\
	$(X_CFLAGS)
libpangox_1_0_la_LIBADD = 			\
	$(PANGO_LIBS)				\
	$(X_LIBS)				\
	$(X_EXTRA_LIBS)

libpangox_1_0_la_SOURCES = 			\
	pangox.c				\
	pangox-private.h   			\
	pangox-fontcache.c 			\
	pangox-fontmap.c			\
	pango-impl-utils.h			\
	basic-x.c				\
	tables-big.i

-include $(top_srcdir)/git.mk
