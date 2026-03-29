# aria-libc

Standard C library wrappers for the [Aria programming language](https://github.com/alternative-intelligence-cp/aria).

## What is aria-libc?

aria-libc provides Aria-native wrappers around C standard library (libc) functions, so you don't have to write `extern` blocks by hand for common operations. It also enables fully static builds via [musl libc](https://musl.libc.org/) for maximum portability.

## Why?

1. **No more extern boilerplate** — Ready-made wrappers for I/O, memory, strings, math, time, process, and networking functions
2. **Static builds** — Link against musl instead of glibc for zero-dependency, fully portable binaries
3. **One canonical source** — Tested, documented libc wrappers the whole ecosystem can depend on

## Status

**v0.1.1** — Networking module (BSD sockets, address resolution). 342 tests passing across 14 test suites (7 modules × 2 link modes).

All 7 modules fully implemented:
- Dynamic linking via glibc (default)
- **Static linking via musl** — zero runtime dependencies

## Architecture

```
aria-libc/
├── musl-1.2.6/          # musl source (for static builds)
├── build/musl/          # musl install (libc.a, crt*.o, headers)
├── shim/                # C bridge layer (Aria FFI ↔ libc)
│   ├── aria_libc_*.c    # Per-module C shims
│   ├── glibc_compat.c   # Compatibility shim for musl+libstdc++
│   └── Makefile
├── src/                 # Aria source (public API wrappers)
├── tests/               # Test files (direct extern + use-import)
├── scripts/             # Build & test automation
│   ├── build_static.sh  # Static build pipeline
│   ├── run_tests_static.sh  # Static test suite runner
│   └── benchmark.sh     # Static vs dynamic benchmarks
└── aria-package.toml    # Package config
```

### Modules

| Module | Functions | Tests |
|--------|-----------|-------|
| **io** | open, close, read, write, seek, stat, mkdir, rmdir, readdir, rename, unlink, errno, strerror, buffer pool | 34 |
| **mem** | alloc, calloc, realloc, release, memcpy, memmove, memset, memcmp, byte/int64/string read/write, pointer_offset | 51 |
| **string** | strlen, strcmp, strstr, strchr, strrchr, strtol, strtod, toupper, tolower, char classification, strdup, substring, from_int/float, concat_to | 52 |
| **math** | sin, cos, tan, asin, acos, atan, atan2, sqrt, pow, exp, log, log10, floor, ceil, round, fabs, fmod, PI, E, to_int, approx_eq, to_string | 46 |
| **time** | time_now, clock, sleep, usleep, time_format, time_format_utc, time_diff | 10 |
| **process** | getenv, setenv, unsetenv, run (system), getpid, getppid, getuid, getgid, getcwd, chdir, errno, strerror | 25 |
| **net** | socket, close, shutdown, bind, listen, accept, connect, send, recv, sendto, recvfrom, setsockopt, getsockopt, set_nonblocking, poll, resolve (getaddrinfo), gethostname, inet_aton/ntoa, htons/ntohs/htonl/ntohl, tcp_connect, tcp_listen | 80 |

### Link Modes

| Mode | Binary Size | Startup | Dependencies |
|------|------------|---------|--------------|
| **Dynamic** (glibc) | ~115 KB | ~8 ms | libc.so.6, libstdc++.so.6, libgcc_s.so.1, libatomic.so.1, libm.so.6 |
| **Static** (musl) | ~1.7 MB | ~5 ms | **None** |

## Quick Start

### Building

```bash
# Build musl (one-time)
cd musl-1.2.6
./configure --prefix=$(pwd)/../build/musl --disable-shared
make -j$(nproc) && make install
cd ..

# Build all shims (shared + static + glibc compat)
cd shim && make && cd ..
```

### Dynamic Build (default)

```bash
ariac myfile.aria -o myfile -L/path/to/aria-libc/shim -laria_libc_io
LD_LIBRARY_PATH=/path/to/aria-libc/shim ./myfile
```

### Static Build (musl)

```bash
# Using the build script:
./scripts/build_static.sh myfile.aria -o myfile_static -Lshim -laria_libc_io

# The binary has zero dependencies:
file myfile_static    # → "statically linked"
ldd myfile_static     # → "not a dynamic executable"
./myfile_static       # Just works, anywhere
```

For programs using multiple modules:
```bash
./scripts/build_static.sh myfile.aria -o myfile_static \
    -Lshim -laria_libc_io -laria_libc_string -laria_libc_mem
```

### Manual Static Build

The `build_static.sh` script automates this 3-step pipeline:

```bash
# 1. Compile Aria → assembly
ariac myfile.aria --emit-asm -o myfile.s

# 2. Assemble
clang -c myfile.s -o myfile.o

# 3. Link against musl + aria runtime + shims
MUSL=build/musl/lib
clang++ -static -nostdlib -nostartfiles \
    $MUSL/crt1.o $MUSL/crti.o \
    myfile.o \
    /path/to/libaria_runtime.a \
    -Lshim -laria_libc_io -lglibc_compat \
    -L/usr/lib/gcc/x86_64-linux-gnu/13 -lstdc++ -lgcc -lgcc_eh -latomic \
    $MUSL/libc.a $MUSL/crtn.o \
    -o myfile_static
```

## Testing

```bash
# Run all tests (dynamic)
cd tests
for t in test_libc_*.aria test_use_*.aria; do
    ariac "$t" -o "${t%.aria}" -L../shim -laria_libc_io -laria_libc_mem \
        -laria_libc_string -laria_libc_math -laria_libc_time -laria_libc_process
    LD_LIBRARY_PATH=../shim "./${t%.aria}"
done

# Run all tests (static, musl-linked)
./scripts/run_tests_static.sh

# Performance benchmarks
./scripts/benchmark.sh
```

## Usage

### With `use` Imports (recommended)

```aria
use "../src/aria_libc_io.aria".*;

func:failsafe = int32(tbb32:err) {
    drop(println("Failsafe triggered"));
    exit(1);
};

func:main = int32() {
    int64:bytes = raw(libc_write_file("/tmp/test.txt", "Hello from Aria!\n"));
    drop(println("Wrote " + bytes + " bytes"));
    exit(0);
};
```

### With Extern Blocks (direct FFI)

```aria
extern "aria_libc_io" {
    func:aria_libc_io_write_file = int64(str:path, str:content);
    func:aria_libc_io_read_file = str(str:path);
    func:aria_libc_io_stat_exists = int64(str:path);
}

func:failsafe = int32(tbb32:err) {
    drop(println("Failsafe triggered"));
    exit(1);
};

func:main = int32() {
    int64:wrote = aria_libc_io_write_file("/tmp/test.txt", "Hello!\n");
    str:content = aria_libc_io_read_file("/tmp/test.txt");
    drop(println("Read: " + content));
    exit(0);
};
```

See `src/aria_libc_*.aria` for the full API reference.

## How Static Linking Works

The static build pipeline replaces glibc with musl libc:

1. **ariac** compiles `.aria` → x86-64 assembly (via LLVM)
2. **clang** assembles → `.o` object file
3. **clang++** links statically:
   - musl's CRT (crt1.o, crti.o, crtn.o) — program entry point
   - musl's libc.a — all C standard library functions
   - libaria_runtime.a — Aria's runtime (GC, strings, threads, etc.)
   - Per-module shim .a files — the C bridge between Aria FFI and libc
   - libglibc_compat.a — bridges glibc-specific symbols that libstdc++ needs
   - GCC static libs — libstdc++.a, libgcc.a, libgcc_eh.a, libatomic.a

The glibc compatibility shim (`shim/glibc_compat.c`) provides ~15 glibc-specific
symbols (fortified functions, LFS aliases, etc.) that libstdc++.a references when
compiled on glibc systems. This lets us use the system's libstdc++ with musl's libc.

## Long-term Vision

The near-term goal is wrapping existing libc (via musl for static builds). The long-term goal is a standalone libc implementation written in Aria that doesn't depend on musl or glibc at all.

## License

Apache License 2.0 — see [LICENSE](LICENSE)

## Part of the Aria Ecosystem

- [Aria Compiler](https://github.com/alternative-intelligence-cp/aria)
- [Aria Packages](https://github.com/alternative-intelligence-cp/aria-packages)
- [Aria Documentation](https://github.com/alternative-intelligence-cp/aria-docs)
