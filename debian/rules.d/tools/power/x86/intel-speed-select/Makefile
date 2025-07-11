include $(top_rulesdir)/Makefile.inc

# Intel Speed Select Tool (ISST)
MAKE_ISST := CFLAGS='$(CFLAGS) $(CPPFLAGS)' LDFLAGS='$(LDFLAGS)' $(MAKE) O=$(CURDIR)

MAKE_ISST += bindir=/usr/sbin V=1

all:
	$(MAKE_ISST) -C $(srcdir)

install:
	$(MAKE_ISST) -C $(srcdir) install DESTDIR=$(DESTDIR)
