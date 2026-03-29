CC      ?= gcc
CFLAGS  := -O2 -Wall -Wextra -fPIC
MUSL_DIR := ../build/musl

# ── Modules ──────────────────────────────────────────────────────────
MODULES := aria_libc_io aria_libc_mem aria_libc_string aria_libc_math aria_libc_time aria_libc_process aria_libc_net

SHARED  := $(patsubst %,lib%.so,$(MODULES))
STATIC  := $(patsubst %,lib%.a,$(MODULES))
OBJECTS := $(patsubst %,%.o,$(MODULES))

.PHONY: all clean dynamic static

all: dynamic static libglibc_compat.a

dynamic: $(SHARED)

static: $(STATIC)

# Pattern rules for all modules
lib%.so: %.c
	$(CC) $(CFLAGS) -shared -o $@ $< -lm

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

lib%.a: %.o
	ar rcs $@ $<

# glibc compat shim for musl static linking
libglibc_compat.a: glibc_compat.o
	ar rcs $@ $<

glibc_compat.o: glibc_compat.c
	$(CC) -O2 -Wall -Wextra -Wno-unused-parameter -c -o $@ $< -I$(MUSL_DIR)/include

clean:
	rm -f $(SHARED) $(STATIC) $(OBJECTS) glibc_compat.o libglibc_compat.a
