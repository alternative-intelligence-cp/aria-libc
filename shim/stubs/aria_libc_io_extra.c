// Extra io shim functions not in the Aria-compiled shim
#include <stdio.h>
#include <errno.h>

long long aria_libc_io_fopen(const char* path, const char* mode) {
    FILE* f = fopen(path, mode);
    return (long long)(unsigned long)f;
}

int aria_libc_io_fclose(long long handle) {
    return fclose((FILE*)(unsigned long)handle);
}

int aria_libc_io_feof(long long handle) {
    return feof((FILE*)(unsigned long)handle);
}

int aria_libc_io_fflush(long long handle) {
    return fflush((FILE*)(unsigned long)handle);
}

long long aria_libc_io_ftell(long long handle) {
    return (long long)ftell((FILE*)(unsigned long)handle);
}

int aria_libc_io_fseek(long long handle, long long offset, int whence) {
    return fseek((FILE*)(unsigned long)handle, (long)offset, whence);
}

int aria_libc_io_seek_end(void) {
    return SEEK_END;
}

long long aria_libc_io_fread(long long buf, long long size, long long count, long long handle) {
    return (long long)fread((void*)(unsigned long)buf, (size_t)size, (size_t)count, (FILE*)(unsigned long)handle);
}

long long aria_libc_io_fwrite(long long buf, long long size, long long count, long long handle) {
    return (long long)fwrite((const void*)(unsigned long)buf, (size_t)size, (size_t)count, (FILE*)(unsigned long)handle);
}

int aria_libc_io_fputs(const char* s, long long handle) {
    return fputs(s, (FILE*)(unsigned long)handle);
}

// Returns a heap-allocated string, caller must free
const char* aria_libc_io_fgets(long long handle, long long max_len) {
    static char buf[4096];
    long long len = max_len < 4096 ? max_len : 4095;
    char* result = fgets(buf, (int)len, (FILE*)(unsigned long)handle);
    return result;
}
