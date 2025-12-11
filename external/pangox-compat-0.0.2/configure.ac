dnl Process this file with autoconf to create configure.
AC_PREREQ(2.56)

AC_INIT(pangox-compat, 0.0.2, [/dev/null])

AC_CONFIG_SRCDIR([pangox.pc.in])

AM_INIT_AUTOMAKE(1.9)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PROG_CC
AM_PROG_LIBTOOL

PKG_CHECK_MODULES(PANGO, pango glib-2.0 >= 2.31.0 gobject-2.0 gmodule-no-export-2.0)

AC_PATH_XTRA
if test x$no_x = xyes ; then
	AC_MSG_ERROR([X development libraries not found])
fi
X_LIBS="$X_LIBS -lX11"

AC_CONFIG_FILES([
Makefile
pangox.pc
])

AC_OUTPUT
