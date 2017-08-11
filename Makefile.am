ACLOCAL_AMFLAGS = -I m4

SUBDIRS= src test tools

if RTR_RPC
SUBDIRS+= rpc
endif

retrace: retrace.in
	@sed -e "s|\@libdir\@|$(libdir)|" retrace.in > $@
	chmod +x $@

dist_bin_SCRIPTS= retrace
pkgdata_DATA= retrace.conf.example

EXTRA_DIST= retrace.in autogen.sh retrace.conf.example
CLEANFILES = $(dist_bin_SCRIPTS)
