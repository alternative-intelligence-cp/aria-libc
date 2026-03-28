# aria-libc

Standard C library wrappers for the [Aria programming language](https://github.com/alternative-intelligence-cp/aria).

## What is aria-libc?

aria-libc provides Aria-native wrappers around C standard library (libc) functions, so you don't have to write `extern` blocks by hand for common operations. It also enables fully static builds via [musl libc](https://musl.libc.org/) for maximum portability.

## Why?

1. **No more extern boilerplate** — Ready-made wrappers for I/O, memory, strings, math, time, and process functions
2. **Static builds** — Link against musl instead of glibc for zero-dependency, fully portable binaries
3. **One canonical source** — Tested, documented libc wrappers the whole ecosystem can depend on

## Status

**v0.0.1** — I/O module complete with dynamic linking. 31/31 tests passing.

- I/O C shim: compiled (zero warnings), `.so` and `.a` both built
- musl 1.2.6: built as static library
- Static linking: deferred to v0.0.2 (blocked by Aria runtime glibc dependency)

> **Note**: Due to compiler bugs BUG-001/BUG-002, `use` imports of pub func wrappers
> don't work yet. Consumers must use the "extern block" pattern — declare the extern
> block directly in your source file (see Usage below).

## Architecture

```
aria-libc/
├── musl-1.2.6/          # musl source (for static builds)
├── shim/                 # C bridge layer (Aria FFI ↔ libc)
├── src/                  # Aria source (public API)
├── tests/               # Test files
└── aria-package.toml    # Package config
```

### Modules

| Module | Status | Functions |
|--------|--------|-----------|
| **io** | **v0.0.1** | open, close, read, write, seek, stat, mkdir, rmdir, readdir, rename, unlink, errno, strerror, buffer pool |
| **mem** | planned | alloc, realloc, free, memcpy, memset, memmove |
| **string** | planned | strlen, strcmp, strcpy, strstr, strtol, strtod, toupper, tolower |
| **math** | planned | sin, cos, sqrt, pow, exp, log, floor, ceil, fabs |
| **time** | planned | time_now, clock, sleep, time_format, time_diff |
| **process** | planned | getenv, setenv, system, exit, getpid |

### Link Modes

- **Dynamic** (default) — Links against system glibc. Works everywhere, smaller binary.
- **Static** — Links against musl libc.a. Zero runtime dependencies, fully portable.

## Building

```bash
# Build musl (one-time, for future static linking)
cd musl-1.2.6
./configure --prefix=$(pwd)/../build/musl --disable-shared
make -j$(nproc) && make install

# Build the I/O shim
cd ../shim
make
```

## Testing

```bash
cd tests
ariac test_libc_io.aria -o test_libc_io -L../shim -laria_libc_io
LD_LIBRARY_PATH=../shim ./test_libc_io
```

Expected output: 31/31 tests PASS across 13 test groups (file write, stat, read,
append, low-level fd ops, buffer-based read, directory listing, rename, unlink,
rmdir, errno).

## Usage

Due to BUG-001/BUG-002, use the extern block pattern (same as aria-sqlite):

```aria
extern "aria_libc_io" {
    func:aria_libc_io_open = int32(str:path, int32:flags);
    func:aria_libc_io_close = int32(int32:fd);
    func:aria_libc_io_write_string = int64(int32:fd, str:data);
    func:aria_libc_io_read_file = str(str:path);
    func:aria_libc_io_stat_exists = int32(str:path);
}

func:failsafe = NIL(int32:code) {
    drop(println("Failsafe triggered: code " + code));
    pass(NIL);
};

func:main = int32() {
    // Write a file
    int32:fd = aria_libc_io_open("/tmp/test.txt", 1i32);
    drop(aria_libc_io_write_string(fd, "Hello from Aria!\n"));
    drop(aria_libc_io_close(fd));

    // Read it back
    str:content = aria_libc_io_read_file("/tmp/test.txt");
    drop(println("Read: " + content));

    pass(0i32);
};
```

Compile:
```bash
ariac myfile.aria -o myfile -L/path/to/aria-libc/shim -laria_libc_io
LD_LIBRARY_PATH=/path/to/aria-libc/shim ./myfile
```

See `src/aria_libc_io.aria` for the full API reference (all exported function signatures).

## Known Issues

- **Static linking** deferred to v0.0.2 — `libaria_runtime.a` depends on glibc symbols
  (`__isoc23_strtoll@@GLIBC_2.38`) incompatible with musl. Also, ariac places `-Wl,`
  flags after `-l` flags (BUG-003), preventing partial static linking.
- **`use` imports broken** for FFI wrappers (BUG-001, BUG-002). Must use extern block
  pattern directly in consumer files.

## Long-term Vision

The near-term goal is wrapping existing libc (via musl for static builds). The long-term goal is a standalone libc implementation written in Aria that doesn't depend on musl or glibc at all.

## License

Apache License 2.0 — see [LICENSE](LICENSE)

## Part of the Aria Ecosystem

- [Aria Compiler](https://github.com/alternative-intelligence-cp/aria)
- [Aria Packages](https://github.com/alternative-intelligence-cp/aria-packages)
- [Aria Documentation](https://github.com/alternative-intelligence-cp/aria-docs)
