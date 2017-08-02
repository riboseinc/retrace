AM_CFLAGS = -Wall
noinst_PROGRAMS = functions2yaml
functions2yaml_SOURCES = functions2yaml.c

lib_LTLIBRARIES = libretracerpc.la
libretracerpc_la_LDFLAGS = -no-undefined -avoid-version
libretracerpc_la_CFLAGS = $(AM_CFLAGS) $(LIBSSL_FLAGS)
noinst_HEADERS = rpc.h shim.h frontend.h fixups.h
libretracerpc_la_SOURCES = rpc.c shim.c version.c fixups.c
bin_PROGRAMS = retrace
retrace_SOURCES = retrace.c version.c handlers.c frontend.c

BUILT_SOURCES = shim.c shim.h version.c handlers.c
CLEANFILES = shim.c shim.h version.c handlers.c

shim.c : functions functions2yaml shim_c.template shim.h
	./functions2yaml <$< | mustache - shim_c.template >$@

shim.h : functions functions2yaml shim_h.template
	./functions2yaml <$< | mustache - shim_h.template >$@

handlers.c : functions functions2yaml handlers_c.template shim.h rpc.h frontend.h
	./functions2yaml <$< | mustache - handlers_c.template >$@

version.c : shim.c shim.h
	echo const char *rpc_version = \"$$(cat shim.c shim.h | md5sum | cut -c1-32)\"\; >$@
