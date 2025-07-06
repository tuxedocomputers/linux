include $(top_rulesdir)/Makefile.inc

# Upstream enables LTO by default, but we don't want it
CFLAGS += -fno-lto

# Use the version of libcpupower that we just built
CPPFLAGS += -I$(top_srcdir)/tools/power/cpupower/lib
LDFLAGS += -L$(CURDIR)/../../power/cpupower

MAKE_RTLA := +CFLAGS='$(CFLAGS) $(CPPFLAGS)' EXTRA_CFLAGS='$(CFLAGS) $(CPPFLAGS)' LDFLAGS='$(LDFLAGS)' $(MAKE) -C $(top_srcdir)/$(OUTDIR) O=$(CURDIR)
MAKE_RTLA += LD='$(CROSS_COMPILE)ld'
MAKE_RTLA += PKG_CONFIG='$(PKG_CONFIG)'
MAKE_RTLA += STRIP=true
MAKE_RTLA += V=1

all:
	$(MAKE_RTLA)

install:
	$(MAKE_RTLA) install
ifneq (,$(filter nodoc,$(DEB_BUILD_PROFILES)))
	rm -rf $(DESTDIR)/usr/share/man
endif

clean:
	rm -rf *
