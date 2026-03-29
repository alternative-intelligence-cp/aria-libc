/*  aria_libc_string.c — Flat-parameter C bridge between Aria FFI and libc string functions
 *
 *  Build (dynamic): cc -O2 -shared -fPIC -Wall -o libaria_libc_string.so aria_libc_string.c
 *  Build (static):  cc -O2 -c -Wall -o aria_libc_string.o aria_libc_string.c
 *                   ar rcs libaria_libc_string.a aria_libc_string.o
 *
 *  Aria ABI rules:
 *    - String params  = const char* (null-terminated)
 *    - String returns = AriaString {char* data, int64_t length} by value
 *    - flt32/flt64    = double at C ABI
 *    - Pointers passed as int64 (cast void* <-> intptr_t)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
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

int64_t aria_libc_string_errno(void) {
    return (int64_t)last_errno;
}

AriaString aria_libc_string_strerror(int64_t errnum) {
    return make_aria_string(strerror((int)errnum));
}

/* ── Length ───────────────────────────────────────────────────────────── */

int64_t aria_libc_string_length(const char *s) {
    if (!s) return 0;
    return (int64_t)strlen(s);
}

/* ── Comparison ──────────────────────────────────────────────────────── */

int64_t aria_libc_string_compare(const char *a, const char *b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return (int64_t)strcmp(a, b);
}

int64_t aria_libc_string_compare_n(const char *a, const char *b, int64_t n) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    if (n <= 0) return 0;
    return (int64_t)strncmp(a, b, (size_t)n);
}

/* ── Copy (into allocated memory buffers via int64 pointer) ──────────── */

int64_t aria_libc_string_copy_to(int64_t dst_ptr, const char *src) {
    if (dst_ptr == 0 || !src) {
        last_errno = EINVAL;
        return -1;
    }
    char *dst = (char *)(intptr_t)dst_ptr;
    strcpy(dst, src);
    last_errno = 0;
    return (int64_t)strlen(src);
}

int64_t aria_libc_string_copy_to_n(int64_t dst_ptr, const char *src, int64_t n) {
    if (dst_ptr == 0 || !src || n <= 0) {
        last_errno = EINVAL;
        return -1;
    }
    char *dst = (char *)(intptr_t)dst_ptr;
    strncpy(dst, src, (size_t)n);
    last_errno = 0;
    /* Return actual bytes copied (min of strlen(src), n) */
    size_t src_len = strlen(src);
    return (int64_t)(src_len < (size_t)n ? src_len : (size_t)n);
}

/* ── Search ──────────────────────────────────────────────────────────── */

int64_t aria_libc_string_find(const char *haystack, const char *needle) {
    if (!haystack || !needle) return -1;
    const char *result = strstr(haystack, needle);
    if (!result) return -1;
    return (int64_t)(result - haystack);
}

int64_t aria_libc_string_find_char(const char *s, int64_t c) {
    if (!s) return -1;
    const char *result = strchr(s, (int)c);
    if (!result) return -1;
    return (int64_t)(result - s);
}

int64_t aria_libc_string_find_last_char(const char *s, int64_t c) {
    if (!s) return -1;
    const char *result = strrchr(s, (int)c);
    if (!result) return -1;
    return (int64_t)(result - s);
}

/* ── Conversion ──────────────────────────────────────────────────────── */

int64_t aria_libc_string_to_int(const char *s) {
    if (!s) {
        last_errno = EINVAL;
        return 0;
    }
    char *endptr = NULL;
    errno = 0;
    long val = strtol(s, &endptr, 10);
    if (errno != 0) {
        last_errno = errno;
        return 0;
    }
    if (endptr == s) {
        last_errno = EINVAL;
        return 0;
    }
    last_errno = 0;
    return (int64_t)val;
}

double aria_libc_string_to_float(const char *s) {
    if (!s) {
        last_errno = EINVAL;
        return 0.0;
    }
    char *endptr = NULL;
    errno = 0;
    double val = strtod(s, &endptr);
    if (errno != 0) {
        last_errno = errno;
        return 0.0;
    }
    if (endptr == s) {
        last_errno = EINVAL;
        return 0.0;
    }
    last_errno = 0;
    return val;
}

/* ── Case conversion (character-level) ───────────────────────────────── */

int64_t aria_libc_string_upper(int64_t c) {
    return (int64_t)toupper((int)c);
}

int64_t aria_libc_string_lower(int64_t c) {
    return (int64_t)tolower((int)c);
}

/* ── Character classification ────────────────────────────────────────── */

int64_t aria_libc_string_is_alpha(int64_t c) {
    return isalpha((int)c) ? 1 : 0;
}

int64_t aria_libc_string_is_digit(int64_t c) {
    return isdigit((int)c) ? 1 : 0;
}

int64_t aria_libc_string_is_alnum(int64_t c) {
    return isalnum((int)c) ? 1 : 0;
}

int64_t aria_libc_string_is_space(int64_t c) {
    return isspace((int)c) ? 1 : 0;
}

/* ── String duplication (returns int64 pointer to malloc'd copy) ─────── */

int64_t aria_libc_string_duplicate(const char *s) {
    if (!s) {
        last_errno = EINVAL;
        return 0;
    }
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (!copy) {
        last_errno = errno;
        return 0;
    }
    memcpy(copy, s, len + 1);
    last_errno = 0;
    return (int64_t)(intptr_t)copy;
}

/* ── String from memory pointer (AriaString from null-terminated ptr) ── */

AriaString aria_libc_string_from_ptr(int64_t ptr) {
    if (ptr == 0) {
        AriaString r = { NULL, 0 };
        return r;
    }
    return make_aria_string((const char *)(intptr_t)ptr);
}

/* ── Concatenation into buffer ───────────────────────────────────────── */

int64_t aria_libc_string_concat_to(int64_t dst_ptr, int64_t dst_size, const char *src) {
    if (dst_ptr == 0 || !src || dst_size <= 0) {
        last_errno = EINVAL;
        return -1;
    }
    char *dst = (char *)(intptr_t)dst_ptr;
    size_t current_len = strlen(dst);
    size_t src_len = strlen(src);
    if (current_len + src_len >= (size_t)dst_size) {
        last_errno = ERANGE;
        return -1;
    }
    memcpy(dst + current_len, src, src_len + 1);
    last_errno = 0;
    return (int64_t)(current_len + src_len);
}

/* ── Substring extraction ────────────────────────────────────────────── */

AriaString aria_libc_string_substring(const char *s, int64_t start, int64_t len) {
    if (!s || start < 0 || len <= 0) {
        AriaString r = { NULL, 0 };
        return r;
    }
    int64_t slen = (int64_t)strlen(s);
    if (start >= slen) {
        AriaString r = { NULL, 0 };
        return r;
    }
    if (start + len > slen) {
        len = slen - start;
    }
    char *copy = (char *)malloc(len + 1);
    if (!copy) {
        AriaString r = { NULL, 0 };
        return r;
    }
    memcpy(copy, s + start, len);
    copy[len] = '\0';
    AriaString r = { copy, len };
    return r;
}

/* ── Integer/Float to string ──────────────────────────────────────────── */

AriaString aria_libc_string_from_int(int64_t val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", (long)val);
    return make_aria_string(buf);
}

AriaString aria_libc_string_from_float(double val) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", val);
    return make_aria_string(buf);
}
