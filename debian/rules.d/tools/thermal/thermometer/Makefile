PROGS = thermometer

installdir = /usr/bin

include $(top_rulesdir)/Makefile.inc

# thermometer does not support OOT build, so we copy the
# source before building.
# thermometer does not support appending user CFLAGS or LDFLAGS,
# so we have to specify the include directory and libraries too.
# Use the static libthermal_tools so we don't have to support it
# as a shared library.
MAKE_THERMOMETER = $(MAKE) \
	CC='$(CC)' CFLAGS='$(CFLAGS) $(CPPFLAGS) -I$(srcdir)/../lib' \
	LD='$(CROSS_COMPILE)-ld' LDFLAGS='$(LDFLAGS) $(CURDIR)/../lib/libthermal_tools.a -lconfig'

$(PROGS):
	rsync -a $(srcdir)/ .
	$(MAKE_THERMOMETER)
