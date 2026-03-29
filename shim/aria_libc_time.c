/*  aria_libc_time.c — Flat-parameter C bridge between Aria FFI and libc time functions
 *
 *  Build (dynamic): cc -O2 -shared -fPIC -Wall -o libaria_libc_time.so aria_libc_time.c
 *  Build (static):  cc -O2 -c -Wall -o aria_libc_time.o aria_libc_time.c
 *                   ar rcs libaria_libc_time.a aria_libc_time.o
 *
 *  Aria ABI rules:
 *    - String params  = const char* (null-terminated)
 *    - String returns = AriaString {char* data, int64_t length} by value
 *    - Pointers passed as int64 (cast void* <-> intptr_t)
 */

#define _DEFAULT_SOURCE
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

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

int64_t aria_libc_time_errno(void) {
    return (int64_t)last_errno;
}

AriaString aria_libc_time_strerror(int64_t errnum) {
    return make_aria_string(strerror((int)errnum));
}

/* ── Core time functions ─────────────────────────────────────────────── */

int64_t aria_libc_time_now(void) {
    time_t t = time(NULL);
    if (t == (time_t)-1) {
        last_errno = errno;
        return 0;
    }
    last_errno = 0;
    return (int64_t)t;
}

int64_t aria_libc_time_clock(void) {
    clock_t c = clock();
    if (c == (clock_t)-1) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return (int64_t)c;
}

/* ── Sleep ───────────────────────────────────────────────────────────── */

int64_t aria_libc_time_sleep(int64_t seconds) {
    if (seconds < 0) {
        last_errno = EINVAL;
        return -1;
    }
    unsigned int remaining = sleep((unsigned int)seconds);
    last_errno = 0;
    return (int64_t)remaining;
}

int64_t aria_libc_time_usleep(int64_t microseconds) {
    if (microseconds < 0) {
        last_errno = EINVAL;
        return -1;
    }
    int result = usleep((useconds_t)microseconds);
    if (result != 0) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return 0;
}

/* ── Formatting ──────────────────────────────────────────────────────── */

AriaString aria_libc_time_format(int64_t epoch, const char *fmt) {
    time_t t = (time_t)epoch;
    struct tm *tm = localtime(&t);
    if (!tm) {
        last_errno = errno;
        return make_aria_string("");
    }
    char buf[256];
    size_t len = strftime(buf, sizeof(buf), fmt, tm);
    if (len == 0) {
        last_errno = EINVAL;
        return make_aria_string("");
    }
    last_errno = 0;
    return make_aria_string(buf);
}

AriaString aria_libc_time_format_utc(int64_t epoch, const char *fmt) {
    time_t t = (time_t)epoch;
    struct tm *tm = gmtime(&t);
    if (!tm) {
        last_errno = errno;
        return make_aria_string("");
    }
    char buf[256];
    size_t len = strftime(buf, sizeof(buf), fmt, tm);
    if (len == 0) {
        last_errno = EINVAL;
        return make_aria_string("");
    }
    last_errno = 0;
    return make_aria_string(buf);
}

/* ── Difference ──────────────────────────────────────────────────────── */

int64_t aria_libc_time_diff(int64_t a, int64_t b) {
    return (int64_t)difftime((time_t)a, (time_t)b);
}
