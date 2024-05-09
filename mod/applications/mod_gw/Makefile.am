include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_gw

mod_LTLIBRARIES = mod_gw.la
mod_gw_la_SOURCES  = mod_gw.c
mod_gw_la_CFLAGS   = $(AM_CFLAGS)
mod_gw_la_CPPFLAGS = $(CURL_CFLAGS) $(AM_CPPFLAGS)
mod_gw_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_gw_la_LDFLAGS  = $(CURL_LIBS) -avoid-version -module -no-undefined -shared
