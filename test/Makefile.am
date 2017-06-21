AM_CFLAGS = -g

dist_check_SCRIPTS = runtests.sh
noinst_PROGRAMS =
TESTS =

if CMOCKA
noinst_PROGRAMS += cmockatest
TESTS += cmockatest
cmockatest_CFLAGS = $(CMOCKA_CFLAGS)
cmockatest_LDADD = $(CMOCKA_LDFLAGS) $(CMOCKA_LIBS)
endif

if ENABLE_TESTS
noinst_PROGRAMS += id env time fork exit malloc str pipe sock sock_srv file trace pledge scanf dlopen
TESTS += $(dist_check_SCRIPTS)
endif
