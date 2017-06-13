CFLAGS	+= -I ../ -fPIC -D_GNU_SOURCE -rdynamic -Wall -g 
LDFLAGS	+= -ldl 

OS              = $(shell uname)
AUTOMATED-TEST  = runtests.sh
CMOCKA-TEST-BIN = cmockatest
OBJS		= $(SRCS:.c=.o)
BINARIES	= id env time fork exit malloc str pipe sock file trace

ifneq ($(OS),Darwin)
	BINARIES += $(CMOCKA-TEST-BIN)
	LDFLAGS += -lcmocka -z text -export-dynamic -fno-builtin
endif


.PHONY: all clean run

all: $(BINARIES) run

$(BINARIES): %: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

run:
ifneq ($(OS),Darwin)
	./$(CMOCKA-TEST-BIN)
endif
	./$(AUTOMATED-TEST)

-include $(SRCS:.c=.d)

$(SRCS:.c=.d):%.d:%.c
	$(CC) $(CFLAGS) -MM $< >$@

clean:
	$(RM) $(OBJS) $(OBJS:.o=.d) $(BINARIES)
