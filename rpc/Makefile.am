noinst_PROGRAMS = functions2yaml
functions2yaml_SOURCES = functions2yaml.c

lib_LTLIBRARIES = libretracerpc.la
libretracerpc_la_LDFLAGS = -no-undefined -avoid-version
libretracerpc_la_CFLAGS = $(AM_CFLAGS) $(LIBSSL_FLAGS)
noinst_HEADERS = rpc.h shim.h
libretracerpc_la_SOURCES = rpc.c shim.c version.c
bin_PROGRAMS = retrace
retrace_SOURCES = retrace.c version.c inspect.c

BUILT_SOURCES = shim.c shim.h version.c inspect.c
CLEANFILES = shim.c shim.h version.c inspect.c

shim.c : functions functions2yaml shim_c.template shim.h
	./functions2yaml <$< | mustache - shim_c.template >$@

shim.h : functions functions2yaml shim_h.template
	./functions2yaml <$< | mustache - shim_h.template >$@

inspect.c : functions functions2yaml inspect_c.template shim.h
	./functions2yaml <$< | mustache - inspect_c.template >$@

version.c : shim.c shim.h
	echo const char *rpc_version = \"$$(cat shim.c shim.h | md5sum | cut -c1-32)\"\; >$@
