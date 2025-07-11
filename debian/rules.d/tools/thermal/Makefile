srcdir := $(top_srcdir)/tools/thermal

include $(top_rulesdir)/Makefile.inc

MAKE_THERMAL_LIB = $(MAKE) -C $(srcdir)/lib \
	O=$(CURDIR)/lib \
	EXTRA_CFLAGS='$(CFLAGS) $(CPPFLAGS)' \
	LDFLAGS='$(LDFLAGS)' \
	SRCARCH=$(ARCH) \
	V=1

# thermometer does not support OOT build, so we copy the
# source before building.
# thermometer does not support appending user CFLAGS or LDFLAGS,
# so we have to specify the include directory and libraries too.
# Use the static libthermal_tools so we don't have to support it
# as a shared library.
MAKE_THERMOMETER = $(MAKE) -C thermometer \
	CC='$(CC)' CFLAGS='$(CFLAGS) $(CPPFLAGS) -I$(srcdir)/lib' \
	LD='$(CROSS_COMPILE)-ld' LDFLAGS='$(LDFLAGS) $(CURDIR)/lib/libthermal_tools.a -lconfig'

# tmon does not support OOT build, so we copy the source before
# building.  Use the make -I option to allow it to include other
# Makefiles in the source tree.
# tmon expects us to set INSTALL_ROOT instead of DESTDIR (?!).
MAKE_TMON = $(MAKE) -C tmon -I $(srcdir)/tmon \
	INSTALL_ROOT=$(DESTDIR) \
	CC='$(CC)' CFLAGS='$(CFLAGS) $(CPPFLAGS)' \
	LDFLAGS='$(LDFLAGS)'

all:
	mkdir -p lib
	rsync -a $(srcdir)/thermometer $(srcdir)/tmon .
# We do not want a shared library, and the shared library link does
# not include LDFLAGS so blhc will complain about it
	$(MAKE_THERMAL_LIB) fixdep
	$(MAKE_THERMAL_LIB) $(CURDIR)/lib/libthermal_tools.a
	$(MAKE_THERMOMETER)
	$(MAKE_TMON)

install:
	install -D -m755 thermometer/thermometer $(DESTDIR)/usr/bin/
	$(MAKE_TMON) install
