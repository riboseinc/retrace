CFLAGS	+= -I ../ -fPIC -D_GNU_SOURCE -rdynamic -Wall -g
LDFLAGS	+= -ldl -lcmocka -z text -export-dynamic

CMOCKA-TEST-BIN	= cmockatest
SRCS		= cmockatest.c
OBJS		= $(SRCS:.c=.o)
BINARIES	= id env time fork exit malloc str pipe sock

.PHONY: all clean run

all: $(BINARIES) run

$(BINARIES): %: %.c
	$(CC) $(CFLAGS) -o $@ $<

$(CMOCKA-TEST-BIN): $(OBJS)
	$(CC) -o $@ $(CFLAGS) $< $(LDFLAGS)

run: $(CMOCKA-TEST-BIN)
	./$(CMOCKA-TEST-BIN)

-include $(SRCS:.c=.d)

$(SRCS:.c=.d):%.d:%.c
	$(CC) $(CFLAGS) -MM $< >$@

clean:
	$(RM) $(CMOCKA-TEST-BIN) $(OBJS) $(OBJS:.o=.d) $(BINARIES)
