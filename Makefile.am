ACLOCAL_AMFLAGS = -I m4

SUBDIRS= src test tools

if RTR_RPC
SUBDIRS+= rpc
endif

pkgdata_DATA= retrace.conf.example
EXTRA_DIST= autogen.sh retrace.conf.example
