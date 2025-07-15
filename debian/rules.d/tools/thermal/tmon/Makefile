include $(top_rulesdir)/Makefile.inc

# tmon does not support OOT build, so we copy the source before
# building.  Use the make -I option to allow it to include other
# Makefiles in the source tree.
# tmon expects us to set INSTALL_ROOT instead of DESTDIR (?!).
MAKE_TMON = $(MAKE) -I $(srcdir) \
	INSTALL_ROOT=$(DESTDIR) \
	CC='$(CC)' CFLAGS='$(CFLAGS) $(CPPFLAGS)' \
	LDFLAGS='$(LDFLAGS)'

all:
	rsync -a $(srcdir)/ .
	$(MAKE_TMON)

install:
	$(MAKE_TMON) install
