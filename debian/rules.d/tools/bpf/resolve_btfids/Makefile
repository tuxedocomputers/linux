PROGS = resolve_btfids

include $(top_rulesdir)/Makefile.inc

resolve_btfids:
	$(MAKE) -C $(top_srcdir)/tools/bpf/resolve_btfids O=$(CURDIR) \
		HOSTCC=$(CC) HOSTCFLAGS='$(CFLAGS) $(CPPFLAGS)' \
		HOSTLD=$(CROSS_COMPILE)ld KBUILD_HOSTLDFLAGS='$(LDFLAGS)' \
		HOSTAR=$(CROSS_COMPILE)ar \
		V=1
