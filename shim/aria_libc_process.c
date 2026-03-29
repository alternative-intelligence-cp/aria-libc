/*  aria_libc_process.c — Flat-parameter C bridge between Aria FFI and libc process/env functions
 *
 *  Build (dynamic): cc -O2 -shared -fPIC -Wall -o libaria_libc_process.so aria_libc_process.c
 *  Build (static):  cc -O2 -c -Wall -o aria_libc_process.o aria_libc_process.c
 *                   ar rcs libaria_libc_process.a aria_libc_process.o
 *
 *  Aria ABI rules:
 *    - String params  = const char* (null-terminated)
 *    - String returns = AriaString {char* data, int64_t length} by value
 *    - "free" in names = shadowed — use _release/_destroy/_cleanup
 *    - Pointers passed as int64 (cast void* <-> intptr_t)
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

/* ── Aria string return type ─────────────────────────────────────────── */

typedef struct {
    char    *data;
    int64_t  length;
} AriaString;

static AriaString make_aria_string(const char *s) {
    if (!s) {
        AriaString r = { NULL, 0 };
        return r;
    }
    int64_t len = (int64_t)strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    AriaString r = { copy, len };
    return r;
}

/* ── Error state ─────────────────────────────────────────────────────── */

static int last_errno = 0;

int64_t aria_libc_process_errno(void) {
    return (int64_t)last_errno;
}

AriaString aria_libc_process_strerror(int64_t errnum) {
    return make_aria_string(strerror((int)errnum));
}

/* ── Errno access (global libc errno) ────────────────────────────────── */

int64_t aria_libc_process_get_errno(void) {
    return (int64_t)errno;
}

AriaString aria_libc_process_get_strerror(int64_t errnum) {
    return make_aria_string(strerror((int)errnum));
}

/* ── Environment ─────────────────────────────────────────────────────── */

AriaString aria_libc_process_getenv(const char *name) {
    if (!name) {
        last_errno = EINVAL;
        return make_aria_string("");
    }
    const char *val = getenv(name);
    if (!val) {
        last_errno = 0;
        return make_aria_string("");
    }
    last_errno = 0;
    return make_aria_string(val);
}

int64_t aria_libc_process_getenv_exists(const char *name) {
    if (!name) {
        return 0;
    }
    const char *val = getenv(name);
    return val ? 1 : 0;
}

int64_t aria_libc_process_setenv(const char *name, const char *value) {
    if (!name || !value) {
        last_errno = EINVAL;
        return -1;
    }
    int result = setenv(name, value, 1);  /* overwrite = 1 */
    if (result != 0) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return 0;
}

int64_t aria_libc_process_unsetenv(const char *name) {
    if (!name) {
        last_errno = EINVAL;
        return -1;
    }
    int result = unsetenv(name);
    if (result != 0) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return 0;
}

/* ── System command ──────────────────────────────────────────────────── */

int64_t aria_libc_process_run(const char *command) {
    if (!command) {
        last_errno = EINVAL;
        return -1;
    }
    int result = system(command);
    if (result == -1) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    /* Return the exit status from WEXITSTATUS if the child exited normally */
    if (WIFEXITED(result)) {
        return (int64_t)WEXITSTATUS(result);
    }
    return (int64_t)result;
}

/* ── Process info ────────────────────────────────────────────────────── */

int64_t aria_libc_process_getpid(void) {
    return (int64_t)getpid();
}

int64_t aria_libc_process_getppid(void) {
    return (int64_t)getppid();
}

int64_t aria_libc_process_getuid(void) {
    return (int64_t)getuid();
}

int64_t aria_libc_process_getgid(void) {
    return (int64_t)getgid();
}

/* ── Working directory ───────────────────────────────────────────────── */

AriaString aria_libc_process_getcwd(void) {
    char buf[4096];
    if (!getcwd(buf, sizeof(buf))) {
        last_errno = errno;
        return make_aria_string("");
    }
    last_errno = 0;
    return make_aria_string(buf);
}

int64_t aria_libc_process_chdir(const char *path) {
    if (!path) {
        last_errno = EINVAL;
        return -1;
    }
    int result = chdir(path);
    if (result != 0) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return 0;
}
