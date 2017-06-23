CFLAGS	+= -I ../ -fPIC -D_GNU_SOURCE -rdynamic -Wall -g -fno-builtin

OS              = $(shell uname)
AUTOMATED-TEST  = runtests.sh
CMOCKA-TEST-BIN = cmockatest
OBJS		= $(SRCS:.c=.o)
BINARIES	= id env time fork exit malloc str pipe sock sock_srv file trace pledge scanf dlopen getaddrinfo

ifeq ($(OS),Linux)
	BINARIES += $(CMOCKA-TEST-BIN)
	LDFLAGS += -lcmocka -z text -export-dynamic -ldl
endif


.PHONY: all clean run

all: $(BINARIES) run

$(BINARIES): %: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

run:
ifeq ($(OS),Linux)
	./$(CMOCKA-TEST-BIN)
endif
	./$(AUTOMATED-TEST)

-include $(SRCS:.c=.d)

$(SRCS:.c=.d):%.d:%.c
	$(CC) $(CFLAGS) -MM $< >$@

clean:
	$(RM) $(OBJS) $(OBJS:.o=.d) $(BINARIES)
