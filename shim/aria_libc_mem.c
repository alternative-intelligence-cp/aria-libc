/*  aria_libc_mem.c — Flat-parameter C bridge between Aria FFI and libc memory functions
 *
 *  Build (dynamic): cc -O2 -shared -fPIC -Wall -o libaria_libc_mem.so aria_libc_mem.c
 *  Build (static):  cc -O2 -c -Wall -o aria_libc_mem.o aria_libc_mem.c
 *                   ar rcs libaria_libc_mem.a aria_libc_mem.o
 *
 *  Aria ABI rules:
 *    - String params  = const char* (null-terminated)
 *    - String returns = AriaString {char* data, int64_t length} by value
 *    - "free" in names = shadowed — use _release/_destroy/_cleanup
 *    - Pointers passed as int64 (cast void* <-> intptr_t)
 */

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

int64_t aria_libc_mem_errno(void) {
    return (int64_t)last_errno;
}

AriaString aria_libc_mem_strerror(int64_t errnum) {
    return make_aria_string(strerror((int)errnum));
}

/* ── Core allocation ─────────────────────────────────────────────────── */

int64_t aria_libc_mem_alloc(int64_t size) {
    if (size <= 0) {
        last_errno = EINVAL;
        return 0;
    }
    void *ptr = malloc((size_t)size);
    if (!ptr) {
        last_errno = errno;
        return 0;
    }
    last_errno = 0;
    return (int64_t)(intptr_t)ptr;
}

int64_t aria_libc_mem_calloc(int64_t count, int64_t size) {
    if (count <= 0 || size <= 0) {
        last_errno = EINVAL;
        return 0;
    }
    void *ptr = calloc((size_t)count, (size_t)size);
    if (!ptr) {
        last_errno = errno;
        return 0;
    }
    last_errno = 0;
    return (int64_t)(intptr_t)ptr;
}

int64_t aria_libc_mem_realloc(int64_t ptr, int64_t new_size) {
    if (new_size <= 0) {
        last_errno = EINVAL;
        return 0;
    }
    void *old = (void *)(intptr_t)ptr;
    void *result = realloc(old, (size_t)new_size);
    if (!result) {
        last_errno = errno;
        return 0;
    }
    last_errno = 0;
    return (int64_t)(intptr_t)result;
}

/* NOTE: Cannot name this "free" — Aria compiler shadows extern names containing "free".
 * Using "release" instead. */
int64_t aria_libc_mem_release(int64_t ptr) {
    if (ptr == 0) {
        return 0;
    }
    free((void *)(intptr_t)ptr);
    last_errno = 0;
    return 0;
}

/* ── Bulk memory operations ──────────────────────────────────────────── */

int64_t aria_libc_mem_copy(int64_t dst, int64_t src, int64_t n) {
    if (dst == 0 || src == 0 || n <= 0) {
        last_errno = EINVAL;
        return 0;
    }
    memcpy((void *)(intptr_t)dst, (const void *)(intptr_t)src, (size_t)n);
    last_errno = 0;
    return dst;
}

int64_t aria_libc_mem_move(int64_t dst, int64_t src, int64_t n) {
    if (dst == 0 || src == 0 || n <= 0) {
        last_errno = EINVAL;
        return 0;
    }
    memmove((void *)(intptr_t)dst, (const void *)(intptr_t)src, (size_t)n);
    last_errno = 0;
    return dst;
}

int64_t aria_libc_mem_set(int64_t dst, int64_t val, int64_t n) {
    if (dst == 0 || n <= 0) {
        last_errno = EINVAL;
        return 0;
    }
    memset((void *)(intptr_t)dst, (int)val, (size_t)n);
    last_errno = 0;
    return dst;
}

int64_t aria_libc_mem_compare(int64_t a, int64_t b, int64_t n) {
    if (a == 0 || b == 0 || n <= 0) {
        last_errno = EINVAL;
        return 0;
    }
    last_errno = 0;
    return (int64_t)memcmp((const void *)(intptr_t)a, (const void *)(intptr_t)b, (size_t)n);
}

/* ── Byte-level read/write (makes allocated memory usable from Aria) ── */

int64_t aria_libc_mem_read_byte(int64_t ptr, int64_t offset) {
    if (ptr == 0) {
        last_errno = EINVAL;
        return -1;
    }
    unsigned char *p = (unsigned char *)(intptr_t)ptr;
    last_errno = 0;
    return (int64_t)p[offset];
}

int64_t aria_libc_mem_write_byte(int64_t ptr, int64_t offset, int64_t val) {
    if (ptr == 0) {
        last_errno = EINVAL;
        return -1;
    }
    unsigned char *p = (unsigned char *)(intptr_t)ptr;
    p[offset] = (unsigned char)(val & 0xFF);
    last_errno = 0;
    return 0;
}

int64_t aria_libc_mem_read_int32(int64_t ptr, int64_t offset) {
    if (ptr == 0) {
        last_errno = EINVAL;
        return 0;
    }
    int32_t *p = (int32_t *)((char *)(intptr_t)ptr + offset);
    last_errno = 0;
    return (int64_t)*p;
}

int64_t aria_libc_mem_read_int64(int64_t ptr, int64_t offset) {
    if (ptr == 0) {
        last_errno = EINVAL;
        return 0;
    }
    int64_t *p = (int64_t *)((char *)(intptr_t)ptr + offset);
    last_errno = 0;
    return *p;
}

int64_t aria_libc_mem_write_int64(int64_t ptr, int64_t offset, int64_t val) {
    if (ptr == 0) {
        last_errno = EINVAL;
        return -1;
    }
    int64_t *p = (int64_t *)((char *)(intptr_t)ptr + offset);
    *p = val;
    last_errno = 0;
    return 0;
}

/* ── String <-> memory bridge ────────────────────────────────────────── */

AriaString aria_libc_mem_read_string(int64_t ptr, int64_t max_len) {
    if (ptr == 0 || max_len <= 0) {
        AriaString r = { NULL, 0 };
        return r;
    }
    const char *src = (const char *)(intptr_t)ptr;
    int64_t len = 0;
    while (len < max_len && src[len] != '\0') {
        len++;
    }
    char *copy = (char *)malloc(len + 1);
    if (!copy) {
        AriaString r = { NULL, 0 };
        return r;
    }
    memcpy(copy, src, len);
    copy[len] = '\0';
    AriaString r = { copy, len };
    return r;
}

int64_t aria_libc_mem_write_string(int64_t ptr, const char *s) {
    if (ptr == 0 || !s) {
        last_errno = EINVAL;
        return -1;
    }
    size_t len = strlen(s);
    memcpy((void *)(intptr_t)ptr, s, len + 1);  /* includes null terminator */
    last_errno = 0;
    return (int64_t)len;
}

/* ── Pointer arithmetic helper ───────────────────────────────────────── */

int64_t aria_libc_mem_offset(int64_t ptr, int64_t offset) {
    if (ptr == 0) {
        return 0;
    }
    return (int64_t)((intptr_t)ptr + offset);
}

/* ── Info ─────────────────────────────────────────────────────────────── */

int64_t aria_libc_mem_ptr_size(void) {
    return (int64_t)sizeof(void *);
}

/* ── String byte access (for Aria code that needs character-level parsing) ── */

int64_t aria_libc_mem_string_byte_at(const char *s, int64_t offset) {
    if (!s || offset < 0) return -1;
    return (int64_t)(unsigned char)s[offset];
}
