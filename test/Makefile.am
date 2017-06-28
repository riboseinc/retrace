AM_CFLAGS = $(TESTS_CFLAGS)

dist_check_SCRIPTS = runtests.sh
noinst_PROGRAMS =
TESTS =

# run comocka tests only on Linux
if CMOCKA
if LINUX
noinst_PROGRAMS += cmockatest
TESTS += cmockatest
cmockatest_CFLAGS = $(CMOCKA_CFLAGS)
cmockatest_LDADD = $(CMOCKA_LDFLAGS) $(CMOCKA_LIBS)
endif
endif

if ENABLE_TESTS
noinst_PROGRAMS += id env time fork exit malloc str pipe sock sock_srv file trace pledge scanf dlopen dir exec printf popen char mmap sbrk getaddrinfo
TESTS += $(dist_check_SCRIPTS)
endif
