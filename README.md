# aria-libc

Pure Aria standard library for systems programming — libc functionality without any C dependencies.

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

## What is aria-libc?

aria-libc provides Aria-native modules for I/O, memory, strings, math, time, process management, networking, POSIX, and filesystem operations. All modules are written in **100% pure Aria** using `sys()` syscall wrappers and compiler runtime externs — no C shim layer required. It also enables fully static builds via [musl libc](https://musl.libc.org/) for maximum portability.

## Why?

1. **Zero C dependencies** — Every module is pure Aria code. No C shims, no extern C libraries to build or link
2. **Static builds** — Use `ariac --static` or the musl pipeline for zero-dependency, fully portable binaries
3. **One canonical source** — Tested, documented standard library the whole ecosystem can depend on

## Status

**v0.3.0** — Standalone Release. 538 tests passing across 18 test suites, all as fully static musl-linked binaries.

**Zero C shim dependencies.** All modules are pure Aria code using `sys()` syscalls and compiler runtime (`libaria_runtime.a`) externs.

## Architecture

```
aria-libc/
├── musl-1.2.6/          # musl source (for static builds)
├── build/musl/          # musl install (libc.a, crt*.o, headers)
├── compat/              # glibc→musl compatibility shim (for libstdc++)
│   └── glibc_compat.c
├── src/                 # Pure Aria source modules
│   ├── _*.aria          # Internal implementation modules
│   └── *.aria           # Public API modules (use-importable)
├── tests/               # Test files
├── scripts/             # Build & test automation
│   ├── build_static.sh  # Static build pipeline
│   └── run_tests_static.sh  # Static test suite runner
└── aria-package.toml    # Package config
```

### Modules

| Module | Description | Tests |
|--------|-------------|-------|
| **errno** | POSIX error codes and errno handling | 23 |
| **identity** | System identity (uid, gid, pid, hostname) | 10 |
| **io_core** | File I/O via sys() — open, read, write, close, seek | 27 |
| **stat** | File stat via sys() — size, permissions, timestamps | 29 |
| **fs_core** | Filesystem ops — mkdir, rmdir, rename, unlink, access | 14 |
| **fs_link** | Symlinks, hardlinks, readlink, realpath | 15 |
| **fs_dir** | Directory creation and management | 11 |
| **fs_readdir** | Directory listing via getdents64 syscall | 22 |
| **mmap** | Memory mapping via sys() — mmap, munmap, mprotect, msync | 16 |
| **alloc** | Memory allocation — alloc, calloc, realloc, release, byte/int64/string R/W | 29 |
| **buf** | Buffer management — ring buffers, byte buffers | 25 |
| **time** | Time and clock via sys() — time_now, clock, nanosleep, formatting | 54 |
| **net** | Networking via sys() — socket, bind, listen, accept, connect, send, recv, poll, DNS | 65 |
| **proc** | Process management via sys() — fork, exec, waitpid, pipes, signals, spawn | 38 |
| **posix_extra** | POSIX extras — random, advanced signal handling, errno table | 61 |
| **str** | String operations — search, parse, buffer, formatting | 79 |
| **math** | Math functions — trig, sqrt, pow, log, floor/ceil/round, fmod, constants | 20 |

### Static Binary Size

All aria-libc programs compile to ~2.2 MB fully static musl-linked binaries with zero runtime dependencies.

## Quick Start

### Building

```bash
# Build musl (one-time, for hermetic static builds)
cd musl-1.2.6
./configure --prefix=$(pwd)/../build/musl --disable-shared
make -j$(nproc) && make install
cd ..

# Build glibc compat shim (one-time)
cd compat && cc -O2 -Wall -c glibc_compat.c -o glibc_compat.o && ar rcs libglibc_compat.a glibc_compat.o && cd ..
```

### Dynamic Build (default)

No special link flags needed — aria-libc modules are `use`-imported directly:

```bash
ariac myfile.aria -o myfile
./myfile
```

### Static Build (simple)

As of ariac v0.2.16+, use the `--static` flag for system static linking:

```bash
ariac --static myfile.aria -o myfile_static
file myfile_static    # → "statically linked"
./myfile_static       # Just works
```

### Static Build (musl — hermetic)

For zero-dependency, hermetic binaries that work on any Linux (no glibc needed):

```bash
# Using the build script:
./scripts/build_static.sh myfile.aria -o myfile_static

# The binary has zero dependencies:
file myfile_static    # → "statically linked"
ldd myfile_static     # → "not a dynamic executable"
./myfile_static       # Works on any x86-64 Linux
```

### Manual Static Build

The `build_static.sh` script automates this 3-step pipeline:

```bash
# 1. Compile Aria → assembly
ariac myfile.aria --emit-asm -o myfile.s

# 2. Assemble
clang -c myfile.s -o myfile.o

# 3. Link against musl + aria runtime
MUSL=build/musl/lib
clang++ -static -nostdlib -nostartfiles \
    $MUSL/crt1.o $MUSL/crti.o \
    myfile.o \
    /path/to/libaria_runtime.a \
    -Lcompat -lglibc_compat \
    -L/usr/lib/gcc/x86_64-linux-gnu/13 -lstdc++ -lgcc -lgcc_eh -latomic \
    $MUSL/libc.a $MUSL/crtn.o \
    -o myfile_static
```

## Testing

```bash
# Run all tests (static, musl-linked)
./scripts/run_tests_static.sh
```

## Usage

### With `use` Imports (recommended)

```aria
use "../src/io_core.aria".*;
use "../src/fs_core.aria".*;

func:failsafe = int32(tbb32:err) {
    drop(println("Failsafe triggered"));
    exit(1);
};

func:main = int32() {
    int64:fd = raw(io_open_write("/tmp/test.txt"));
    drop(raw(io_write_string(fd, "Hello from Aria!\n")));
    drop(raw(io_close(fd)));
    drop(println("Done"));
    exit(0);
};
```

See `src/` for the full set of available modules and their public functions.

## How Static Linking Works

The static build pipeline replaces glibc with musl libc:

1. **ariac** compiles `.aria` → x86-64 assembly (via LLVM)
2. **clang** assembles → `.o` object file
3. **clang++** links statically:
   - musl's CRT (crt1.o, crti.o, crtn.o) — program entry point
   - musl's libc.a — all C standard library functions
   - libaria_runtime.a — Aria's runtime (GC, strings, threads, math, memory primitives)
   - libglibc_compat.a — bridges glibc-specific symbols that libstdc++ needs
   - GCC static libs — libstdc++.a, libgcc.a, libgcc_eh.a, libatomic.a

The glibc compatibility shim (`compat/glibc_compat.c`) provides ~15 glibc-specific
symbols (fortified functions, LFS aliases, etc.) that libstdc++.a references when
compiled on glibc systems. This lets us use the system's libstdc++ with musl's libc.

## Architecture

As of v0.3.0, aria-libc is a **standalone** pure Aria library:

- **System calls** are made directly via Aria's `sys()` built-in — no C wrapper functions needed
- **Memory primitives** (byte/word read/write, memcpy, memset, memcmp) are provided by the compiler runtime (`libaria_runtime.a`)
- **Math functions** (sin, cos, sqrt, etc.) are provided by the compiler runtime
- **String conversions** (to_float, from_float, from_int) are provided by the compiler runtime
- **Everything else** (networking, process management, filesystem, string parsing, buffers, time formatting) is implemented in pure Aria

The only C code remaining is `compat/glibc_compat.c`, which is infrastructure for musl static linking (not an aria-libc dependency).

## Installation

### From source (recommended)

```bash
git clone https://github.com/alternative-intelligence-cp/aria-libc
cd aria-libc

# Build musl (one-time, for hermetic static builds)
cd musl-1.2.6
./configure --prefix=$(pwd)/../build/musl --disable-shared
make -j$(nproc) && make install
cd ..

# Build glibc compat shim
cd compat && cc -O2 -Wall -c glibc_compat.c -o glibc_compat.o && ar rcs libglibc_compat.a glibc_compat.o && cd ..
```

### From aria package registry

```bash
aria-pkg install aria-libc
```

## License

Apache License 2.0 — see [LICENSE](LICENSE)

## Part of the Aria Ecosystem

| Project | Description |
|---------|-------------|
| [Aria Compiler](https://github.com/alternative-intelligence-cp/aria) | The Aria programming language compiler (ariac) |
| [aria-libc](https://github.com/alternative-intelligence-cp/aria-libc) | C standard library wrappers (this repo) |
| [Aria Packages](https://github.com/alternative-intelligence-cp/aria-packages) | Package registry — 80+ packages |
| [Aria Documentation](https://github.com/alternative-intelligence-cp/aria-docs) | Language documentation and tutorials |
| [aria-make](https://github.com/alternative-intelligence-cp/aria-make) | Build system for Aria projects |
| [aria-tools](https://github.com/alternative-intelligence-cp/aria-tools) | aria-safety, aria-mcp, developer tools |
| [AriaX](https://github.com/alternative-intelligence-cp/ariax) | AriaX Linux distribution with Aria integration |
