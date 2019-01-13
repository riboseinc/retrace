
# retrace V2 library

lib_LTLIBRARIES = libretrace_v2.la

libretrace_v2_la_SOURCES = \
	prototypes/locale.c \
	prototypes/ctype.c  \
	prototypes/signal.c \
	prototypes/stdio.c  \
	prototypes/stdlib.c \
	prototypes/uio.c    \
	prototypes/unistd.c \
	actions/basic.c     \
	actions/memfuzz.c   \
	datatypes/basic.c   \
	datatypes/uio.c     \
	datatypes/unistd.c  \
	actions.c           \
	data_types.c        \
	engine.c            \
	funcs.c             \
	main.c              \
	parson.c            \
	real_impls.c        \
	conf.c              \
	logger.c            \
	funcs_symbols.S

libretrace_v2_la_CFLAGS = -Wall -Wextra -shared -fPIC -Wno-unused-parameter

if DARWIN
libretrace_v2_la_CFLAGS += -D_FORTIFY_SOURCE=0 -I./arch/x86-64/osx
libretrace_v2_la_CCASFLAGS = -I./arch/x86-64/osx
libretrace_v2_la_LDFLAGS = -dynamiclib -g
libretrace_v2_la_SOURCES += \
	arch/x86-64/osx/arch_spec_bottom.c \
	arch/x86-64/osx/arch_spec_top.S
endif

if LINUX
libretrace_v2_la_SOURCES += \
	arch/x86-64/linux/arch_spec_bottom.c \
	arch/x86-64/linux/arch_spec_top.S

libretrace_v2_la_CFLAGS += -I./arch/x86-64/linux
libretrace_v2_la_CCASFLAGS = -I./arch/x86-64/linux
libretrace_v2_la_LDFLAGS = -Wl,--no-as-needed -Wl,-ldl
endif

if FREEBSD
libretrace_v2_la_CFLAGS += -I./arch/x86-64/bsd
libretrace_v2_la_CCASFLAGS = -I./arch/x86-64/bsd
libretrace_v2_la_LDFLAGS = -gdwarf-2 -Wl,--no-as-needed -pthread -shared
libretrace_v2_la_SOURCES += \
	arch/x86-64/bsd/arch_spec_bottom.c \
	arch/x86-64/bsd/arch_spec_top.S
endif

# retrace V2 binary

if RTR_V2_WRAPPER

bin_PROGRAMS=retrace2

retrace2_SOURCES = \
	retrace_v2.nos.c \
	retrace_v2.c

retrace_v2.nos.c: retrace_v2.nos
	$(NEREON_TO_CC) retrace_v2.nos ./ rtr2

clean-local:
	rm -rf retrace_v2.nos.c retrace_v2.nos.h

retrace2_CFLAGS = $(NEREON_INCLUDES) -Wall -Wextra
retrace2_LDADD = $(NEREON_LIBDIR)/libnereon.a

endif
