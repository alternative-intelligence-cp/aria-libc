#!/usr/bin/env bash
# Build all C stub shims and merge/create archives
set -euo pipefail

SHIM_DIR="$(cd "$(dirname "$0")" && pwd)"
STUB_DIR="$SHIM_DIR/stubs"

CC="${CC:-gcc}"
CFLAGS="-O2 -Wall -fPIC"

echo "=== Compiling stub objects ==="
for src in "$STUB_DIR"/*.c; do
    name=$(basename "$src" .c)
    echo "  CC  $name"
    $CC $CFLAGS -c "$src" -o "$STUB_DIR/$name.o" -lpthread
done

echo "=== Merging extras into existing archives ==="
# io extra
ar rcs "$SHIM_DIR/libaria_libc_io.a" "$STUB_DIR/aria_libc_io_extra.o"
# math extra
ar rcs "$SHIM_DIR/libaria_libc_math.a" "$STUB_DIR/aria_libc_math_extra.o"
# mem extra
ar rcs "$SHIM_DIR/libaria_libc_mem.a" "$STUB_DIR/aria_libc_mem_extra.o"
# net extra
ar rcs "$SHIM_DIR/libaria_libc_net.a" "$STUB_DIR/aria_libc_net_extra.o"
# process extra
ar rcs "$SHIM_DIR/libaria_libc_process.a" "$STUB_DIR/aria_libc_process_extra.o"

echo "=== Creating new shim archives ==="
ar rcs "$SHIM_DIR/libaria_libc_thread.a" "$STUB_DIR/aria_libc_thread.o"
ar rcs "$SHIM_DIR/libaria_libc_mutex.a" "$STUB_DIR/aria_libc_mutex.o"
ar rcs "$SHIM_DIR/libaria_libc_channel.a" "$STUB_DIR/aria_libc_channel.o"
ar rcs "$SHIM_DIR/libaria_libc_hexstream.a" "$STUB_DIR/aria_libc_hexstream.o"
ar rcs "$SHIM_DIR/libaria_libc_pool.a" "$STUB_DIR/aria_libc_pool.o"
ar rcs "$SHIM_DIR/libaria_libc_actor.a" "$STUB_DIR/aria_libc_actor.o"
ar rcs "$SHIM_DIR/libaria_libc_shm.a" "$STUB_DIR/aria_libc_shm.o"
ar rcs "$SHIM_DIR/libaria_libc_rwlock.a" "$STUB_DIR/aria_libc_rwlock.o"

echo "=== Done! Archive listing ==="
ls -la "$SHIM_DIR"/libaria_libc_*.a
