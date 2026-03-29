/*  aria_libc_math.c — Flat-parameter C bridge between Aria FFI and libc math functions
 *
 *  Build (dynamic): cc -O2 -shared -fPIC -Wall -o libaria_libc_math.so aria_libc_math.c -lm
 *  Build (static):  cc -O2 -c -Wall -o aria_libc_math.o aria_libc_math.c
 *                   ar rcs libaria_libc_math.a aria_libc_math.o
 *
 *  Aria ABI rules:
 *    - String params  = const char* (null-terminated)
 *    - String returns = AriaString {char* data, int64_t length} by value
 *    - flt32/flt64 both pass as double at C ABI
 *    - Pointers passed as int64 (cast void* <-> intptr_t)
 */

#define _DEFAULT_SOURCE
#include <math.h>
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

int64_t aria_libc_math_errno(void) {
    return (int64_t)last_errno;
}

AriaString aria_libc_math_strerror(int64_t errnum) {
    return make_aria_string(strerror((int)errnum));
}

/* ── Constants ───────────────────────────────────────────────────────── */

double aria_libc_math_pi(void) {
    return M_PI;
}

double aria_libc_math_e(void) {
    return M_E;
}

/* ── Trigonometric ───────────────────────────────────────────────────── */

double aria_libc_math_sin(double x) {
    errno = 0;
    double r = sin(x);
    last_errno = errno;
    return r;
}

double aria_libc_math_cos(double x) {
    errno = 0;
    double r = cos(x);
    last_errno = errno;
    return r;
}

double aria_libc_math_tan(double x) {
    errno = 0;
    double r = tan(x);
    last_errno = errno;
    return r;
}

double aria_libc_math_asin(double x) {
    errno = 0;
    double r = asin(x);
    last_errno = errno;
    return r;
}

double aria_libc_math_acos(double x) {
    errno = 0;
    double r = acos(x);
    last_errno = errno;
    return r;
}

double aria_libc_math_atan(double x) {
    errno = 0;
    double r = atan(x);
    last_errno = errno;
    return r;
}

double aria_libc_math_atan2(double y, double x) {
    errno = 0;
    double r = atan2(y, x);
    last_errno = errno;
    return r;
}

/* ── Power / Exponential ─────────────────────────────────────────────── */

double aria_libc_math_sqrt(double x) {
    errno = 0;
    double r = sqrt(x);
    last_errno = errno;
    return r;
}

double aria_libc_math_pow(double base, double exponent) {
    errno = 0;
    double r = pow(base, exponent);
    last_errno = errno;
    return r;
}

double aria_libc_math_exp(double x) {
    errno = 0;
    double r = exp(x);
    last_errno = errno;
    return r;
}

double aria_libc_math_log(double x) {
    errno = 0;
    double r = log(x);
    last_errno = errno;
    return r;
}

double aria_libc_math_log10(double x) {
    errno = 0;
    double r = log10(x);
    last_errno = errno;
    return r;
}

/* ── Rounding ────────────────────────────────────────────────────────── */

double aria_libc_math_floor(double x) {
    return floor(x);
}

double aria_libc_math_ceil(double x) {
    return ceil(x);
}

double aria_libc_math_round(double x) {
    return round(x);
}

double aria_libc_math_fabs(double x) {
    return fabs(x);
}

double aria_libc_math_fmod(double x, double y) {
    if (y == 0.0) {
        last_errno = EDOM;
        return 0.0;
    }
    errno = 0;
    double r = fmod(x, y);
    last_errno = errno;
    return r;
}

/* ── Utility ─────────────────────────────────────────────────────────── */

int64_t aria_libc_math_to_int(double x) {
    return (int64_t)x;
}

int64_t aria_libc_math_approx_eq(double a, double b, double epsilon) {
    return fabs(a - b) < epsilon ? 1 : 0;
}

AriaString aria_libc_math_to_string(double x) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.10g", x);
    return make_aria_string(buf);
}
