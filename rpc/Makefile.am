AM_CFLAGS = -Wall -Wno-sizeof-array-argument
noinst_PROGRAMS = functions2yaml
functions2yaml_SOURCES = functions2yaml.c

lib_LTLIBRARIES = libretracerpc.la
libretracerpc_la_LDFLAGS = -no-undefined -avoid-version
libretracerpc_la_CFLAGS = $(AM_CFLAGS) $(LIBSSL_FLAGS)
noinst_HEADERS = rpc.h shim.h frontend.h backend.h functions.h
libretracerpc_la_SOURCES = shim.c version.c backend.c
bin_PROGRAMS = retrace
retrace_SOURCES = retrace.c version.c handlers.c frontend.c functions.c display.c

if LINUX
MD5SUM=/usr/bin/md5sum
endif
if DARWIN
MD5SUM=/sbin/md5
endif
if OPENBSD
MD5SUM=/bin/md5
endif

if !OPENBSD
MUSTACHE=mustache
endif
if OPENBSD
MUSTACHE=mustache22
endif

FUNCTIONS_DEPS= shim.c shim.h handlers.c functions.c functions.h

BUILT_SOURCES= $(FUNCTIONS_DEPS) version.c
CLEANFILES= $(BUILT_SOURCES) functions.yaml

shim.h: shim.h.template
shim.c: shim.c.template
functions.h: functions.h.template
functions.c: functions.c.template
handlers.c : handlers.c.template

$(FUNCTIONS_DEPS): functions.yaml

$(FUNCTIONS_DEPS):
	$(MUSTACHE) - $@.template <functions.yaml >$@

version.c : $(FUNCTION_DEPS)
	echo const char *retrace_version = \"$$(cat $(FUNCTIONS_DEPS) | $(MD5SUM) | cut -c1-32)\"\; >$@

functions.yaml : functions.txt functions2yaml
	./functions2yaml <functions.txt >$@
