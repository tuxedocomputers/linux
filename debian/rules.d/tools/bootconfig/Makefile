include $(top_rulesdir)/Makefile.inc

# bootconfig does not support appending user CFLAGS or LDFLAGS,
# so we have to specify the include directory too.
MAKE_BOOTCONFIG := $(MAKE) -C $(top_srcdir)/tools/bootconfig \
	O=$(CURDIR) \
	CFLAGS='$(CFLAGS) $(CPPFLAGS) -Iinclude $(LDFLAGS)'

# Default target (all) includes running tests.  If this is not wanted
# (e.g. for cross-builds), build the bootconfig target instead.
ifeq ($(filter nocheck,$(DEB_BUILD_OPTIONS)),)
BUILD_TARGET := all
else
BUILD_TARGET := $(CURDIR)/bootconfig
endif

all:
	$(MAKE_BOOTCONFIG) $(BUILD_TARGET)

install:
	mkdir -p $(DESTDIR)/usr/bin
	$(MAKE_BOOTCONFIG) install
