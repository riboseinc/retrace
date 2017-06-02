CFLAGS	= -I ../
LDFLAGS	= -lcmocka -ldl

CMOCKA-TEST-BIN		= cmockatest
SRCS			= cmockatest.c
OBJS			= $(SRCS:.c=.o)

.PHONY: all clean run

all: run

$(CMOCKA-TEST-BIN): $(OBJS)

run: $(CMOCKA-TEST-BIN)
	./$(CMOCKA-TEST-BIN)

clean:
	$(RM) $(CMOCKA-TEST-BIN) $(OBJS)
