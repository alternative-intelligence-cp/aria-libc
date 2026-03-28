CC      ?= gcc
CFLAGS  := -O2 -Wall -Wextra -fPIC
MUSL_DIR := ../build/musl

# Targets
SHARED  := libaria_libc_io.so
STATIC  := libaria_libc_io.a
OBJECT  := aria_libc_io.o
SRC     := aria_libc_io.c

.PHONY: all clean dynamic static

all: dynamic static

dynamic: $(SHARED)

static: $(STATIC)

$(SHARED): $(SRC)
	$(CC) $(CFLAGS) -shared -o $@ $<

$(OBJECT): $(SRC)
	$(CC) $(CFLAGS) -c -o $@ $<

$(STATIC): $(OBJECT)
	ar rcs $@ $<

clean:
	rm -f $(SHARED) $(STATIC) $(OBJECT)
