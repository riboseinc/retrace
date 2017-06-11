OS 		= $(shell uname)
LD		= ld
GCC		= gcc
RM		= rm -f

ifeq ($(OS),Darwin)
	RETRACE_CFLAGS = $(CFLAGS) -fPIC -D_GNU_SOURCE -Wall
else
	RETRACE_CFLAGS  = $(CFLAGS) -fPIC -D_GNU_SOURCE -rdynamic -Wall
endif

# assume Sierra for now to silence ld warnings
ifeq ($(OS),Darwin)
	export MACOSX_DEPLOYMENT_TARGET = 10.12
endif

ifeq ($(OS),Darwin)
	RETRACE_LDFLAGS = $(LDFLAGS) -dylib
else
	RETRACE_LDFLAGS = $(LDFLAGS) -G -z text --export-dynamic
endif

RETRACE_LIBS	= -ldl -lncurses

ifeq ($(OS),Darwin)
	RETRACE_SO	= retrace.dylib
else
	RETRACE_SO     = retrace.so
endif

SRCS		+= exit.c
SRCS		+= perror.c
SRCS		+= time.c
SRCS		+= char.c
SRCS		+= env.c
SRCS		+= exec.c
SRCS		+= file.c
SRCS		+= common.c
SRCS		+= id.c
SRCS		+= sock.c
SRCS		+= str.c
SRCS		+= read.c
SRCS		+= write.c
SRCS		+= malloc.c
SRCS		+= fork.c
SRCS		+= popen.c
SRCS		+= pipe.c
SRCS		+= dir.c
SRCS		+= printf.c
SRCS		+= select.c
SRCS    += trace.c
OBJS		= $(SRCS:.c=.o)

.PHONY: all clean test

all: $(RETRACE_SO)

$(RETRACE_SO): $(OBJS)
	$(LD) $(RETRACE_LDFLAGS) -o $@ $(RETRACE_LIBS) $^

-include $(SRCS:.c=.d)

$(SRCS:.c=.d):%.d:%.c
	$(CC) $(RETRACE_CFLAGS) -MM $< >$@

%.o: %.d
	$(CC) -c $(RETRACE_CFLAGS) $(@:.o=.c) -o $@

test: $(RETRACE_SO)
	$(MAKE) -C test

clean:
	$(MAKE) -C test clean
	$(RM) $(RETRACE_SO) $(OBJS) $(SRCS:.c=.d)

install:
	mkdir -p $(DEST_DIR)/usr/bin
	mkdir -p $(DEST_DIR)/usr/lib64

	install -m 755 retrace.sh $(DEST_DIR)/usr/bin
	install -c $(RETRACE_SO) $(DEST_DIR)/usr/lib64
