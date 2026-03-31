/*
 * glibc_compat.c — Compatibility shim for linking glibc-built libstdc++
 * against musl libc. Provides glibc-specific symbols that musl doesn't have.
 *
 * These are "fortified" wrappers and glibc extensions that libstdc++.a
 * references when compiled on glibc systems. Each one delegates to the
 * standard POSIX/C function that musl provides.
 */

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

/* ---- glibc __libc_single_threaded ---- */
/* glibc uses this to optimize single-threaded paths. We say multi-threaded (0)
   which is always safe — it just skips optimistic non-atomic paths. */
char __libc_single_threaded = 0;

/* ---- __dso_handle ---- */
/* Used by __cxa_atexit for shared library cleanup. For static bins, just
   provide a valid address. */
void *__dso_handle = &__dso_handle;

/* ---- glibc fortified functions ---- */
/* These are __*_chk variants that glibc provides for _FORTIFY_SOURCE.
   We just forward to the real functions since the buffer size check
   is a compile-time / runtime-abort concern we don't need in static builds. */

void *__memcpy_chk(void *dest, const void *src, size_t len, size_t destlen) {
    (void)destlen;
    return memcpy(dest, src, len);
}

size_t __mbsrtowcs_chk(wchar_t *dest, const char **src, size_t len,
                        mbstate_t *ps, size_t destlen) {
    (void)destlen;
    return mbsrtowcs(dest, src, len, ps);
}

size_t __mbsnrtowcs_chk(wchar_t *dest, const char **src, size_t nms,
                         size_t len, mbstate_t *ps, size_t destlen) {
    (void)destlen;
    return mbsnrtowcs(dest, src, nms, len, ps);
}

wchar_t *__wmemcpy_chk(wchar_t *dest, const wchar_t *src, size_t n,
                        size_t destlen) {
    (void)destlen;
    return wmemcpy(dest, src, n);
}

wchar_t *__wmemset_chk(wchar_t *dest, wchar_t c, size_t n, size_t destlen) {
    (void)destlen;
    return wmemset(dest, c, n);
}

ssize_t __read_chk(int fd, void *buf, size_t nbytes, size_t buflen) {
    (void)buflen;
    return read(fd, buf, nbytes);
}

int __sprintf_chk(char *str, int flag, size_t slen, const char *fmt, ...) {
    (void)flag;
    (void)slen;
    va_list ap;
    va_start(ap, fmt);
    int ret = vsprintf(str, fmt, ap);
    va_end(ap);
    return ret;
}

/* ---- glibc ISO C23 strtol/strtoul variants ---- */
unsigned long __isoc23_strtoul(const char *nptr, char **endptr, int base) {
    return strtoul(nptr, endptr, base);
}

long long __isoc23_strtoll(const char *nptr, char **endptr, int base) {
    return strtoll(nptr, endptr, base);
}

/* ---- 64-bit file ops (glibc LFS) ---- */
/* On musl (and any 64-bit Linux), off_t is already 64-bit.
   These are identical to their non-64 counterparts. */
FILE *fopen64(const char *path, const char *mode) {
    return fopen(path, mode);
}

int fseeko64(FILE *stream, off_t offset, int whence) {
    return fseeko(stream, offset, whence);
}

off_t ftello64(FILE *stream) {
    return ftello(stream);
}

int fstat64(int fd, struct stat64 *buf) {
    return fstat(fd, (struct stat *)buf);
}

off_t lseek64(int fd, off_t offset, int whence) {
    return lseek(fd, offset, whence);
}

/* ---- arc4random ---- */
/* glibc 2.36+ provides arc4random. musl doesn't have it yet.
   Use getrandom(2) syscall via musl's <sys/random.h>. */
#include <sys/random.h>

unsigned int arc4random(void) {
    unsigned int val;
    /* getrandom with no flags blocks until entropy is available — safe */
    while (getrandom(&val, sizeof(val), 0) != sizeof(val)) {
        /* retry on EINTR */
    }
    return val;
}

/* ---- _dl_find_object ---- */
/* Used by libgcc_eh for DWARF exception unwinding optimization (glibc 2.35+).
   Returning non-zero means "not found" — the unwinder falls back to
   dl_iterate_phdr which musl supports. */
int _dl_find_object(void *pc, void *result) {
    (void)pc;
    (void)result;
    return -1;  /* not found — use fallback */
}
