// Extra string shim functions
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// aria_libc_string_from_buf(buf_ptr, byte_offset, len) → new heap string
const char* aria_libc_string_from_buf(const void* buf, int64_t offset, int64_t len) {
    const char* src = (const char*)buf + offset;
    char* s = (char*)malloc((size_t)len + 1);
    if (!s) return "";
    memcpy(s, src, (size_t)len);
    s[len] = '\0';
    return s;
}

// aria_libc_string_copy_to_buf(dst_ptr, dst_off, src_str, src_off, len) → void
// Copies len bytes from src[src_off..] to dst[dst_off..]
void aria_libc_string_copy_to_buf(void* dst, int64_t dst_off, const char* src, int64_t src_off, int64_t len) {
    if (!dst || !src || len <= 0) return;
    memcpy((char*)dst + dst_off, src + src_off, (size_t)len);
}

// aria_libc_string_byte_at uses the standard string byte accessor

int64_t aria_libc_string_byte_at(const char* s, int64_t index) {
    if (!s || index < 0) return -1;
    size_t len = strlen(s);
    if ((size_t)index >= len) return -1;
    return (int64_t)(unsigned char)s[index];
}

int64_t aria_libc_string_atoi(const char* s) {
    if (!s) return 0;
    return (int64_t)atoll(s);
}
