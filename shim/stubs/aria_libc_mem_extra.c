// aria_libc_mem — All memory shim functions for Aria stdlib
// Covers: malloc/calloc/realloc/free, byte/i32/i64 read/write with offset,
//         memcpy/memmove/memset wrappers, getenv, string helpers
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ── AriaString return ABI: {const char*, int64_t} returned in rax+rdx ──
typedef struct { const char* data; int64_t length; } AriaString;

// ── Allocators ──────────────────────────────────────────────────────────
void* aria_libc_mem_malloc(int64_t size)                  { return malloc((size_t)size); }
void* aria_libc_mem_calloc(int64_t count, int64_t size)   { return calloc((size_t)count, (size_t)size); }
void* aria_libc_mem_realloc(void* ptr, int64_t size)      { return realloc(ptr, (size_t)size); }
void  aria_libc_mem_free(void* ptr)                       { free(ptr); }

// ── Bulk memory ops ─────────────────────────────────────────────────────
void aria_libc_mem_zero(void* ptr, int64_t n) { memset(ptr, 0, (size_t)n); }
void aria_libc_mem_copy(void* dst, void* src, int64_t n)  { memcpy(dst, src, (size_t)n); }
void aria_libc_mem_move(void* dst, void* src, int64_t n)  { memmove(dst, src, (size_t)n); }

// ── Byte read/write with offset ─────────────────────────────────────────
int32_t aria_libc_mem_read_byte(void* ptr, int64_t offset) {
    if (!ptr) return -1;
    return (int32_t)(*(uint8_t*)((char*)ptr + offset));
}

void aria_libc_mem_write_byte(void* ptr, int64_t offset, int32_t val) {
    if (!ptr) return;
    *(uint8_t*)((char*)ptr + offset) = (uint8_t)(val & 0xFF);
}

// ── i32 read/write with offset ──────────────────────────────────────────
int32_t aria_libc_mem_read_i32(void* ptr, int64_t offset) {
    int32_t val;
    memcpy(&val, (char*)ptr + offset, sizeof(int32_t));
    return val;
}

void aria_libc_mem_write_i32(void* ptr, int64_t offset, int32_t val) {
    memcpy((char*)ptr + offset, &val, sizeof(int32_t));
}

// ── i64 read/write with offset ──────────────────────────────────────────
int64_t aria_libc_mem_read_i64(void* ptr, int64_t offset) {
    int64_t val;
    memcpy(&val, (char*)ptr + offset, sizeof(int64_t));
    return val;
}

void aria_libc_mem_write_i64(void* ptr, int64_t offset, int64_t val) {
    memcpy((char*)ptr + offset, &val, sizeof(int64_t));
}

// ── Environment ─────────────────────────────────────────────────────────
const char* aria_libc_mem_getenv(const char* name) {
    const char* val = getenv(name);
    return val ? val : "";
}

// ── String helpers ──────────────────────────────────────────────────────
// make_string: create AriaString from raw memory at src_ptr+offset, length len
AriaString aria_libc_mem_make_string(void* src_ptr, int64_t offset, int64_t len) {
    AriaString result;
    if (!src_ptr || len <= 0) {
        result.data = "";
        result.length = 0;
        return result;
    }
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) {
        result.data = "";
        result.length = 0;
        return result;
    }
    memcpy(buf, (char*)src_ptr + offset, (size_t)len);
    buf[len] = '\0';
    result.data = buf;
    result.length = len;
    return result;
}

// copy_string: copy string src bytes into dst_ptr at byte offset
void aria_libc_mem_copy_string(void* dst_ptr, int64_t offset, const char* src, int64_t len) {
    if (!dst_ptr || !src || len <= 0) return;
    memcpy((char*)dst_ptr + offset, src, (size_t)len);
}
