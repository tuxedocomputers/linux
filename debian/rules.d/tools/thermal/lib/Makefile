include $(top_rulesdir)/Makefile.inc

MAKE_THERMAL_LIB = $(MAKE) -C $(srcdir) \
	O=$(CURDIR) \
	EXTRA_CFLAGS='$(CFLAGS) $(CPPFLAGS)' \
	LDFLAGS='$(LDFLAGS)' \
	SRCARCH=$(ARCH) \
	V=1

all:
# We do not want a shared library, and the shared library link does
# not include LDFLAGS so blhc will complain about it
	$(MAKE_THERMAL_LIB) fixdep
	$(MAKE_THERMAL_LIB) $(CURDIR)/libthermal_tools.a
