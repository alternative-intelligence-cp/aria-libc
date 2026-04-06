// aria-libc shared memory shim
#include <sys/mman.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct {
    void* ptr;
    int64_t size;
    int fd;
} shm_t;

int64_t aria_libc_shm_create(int64_t size) {
    shm_t* s = (shm_t*)calloc(1, sizeof(shm_t));
    if (!s) return 0;
    s->size = size;
    s->fd = -1;
    s->ptr = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (s->ptr == MAP_FAILED) { free(s); return 0; }
    return (int64_t)(unsigned long)s;
}

void aria_libc_shm_destroy(int64_t handle) {
    shm_t* s = (shm_t*)(unsigned long)handle;
    if (s->ptr && s->ptr != MAP_FAILED) munmap(s->ptr, (size_t)s->size);
    free(s);
}

int64_t aria_libc_shm_size(int64_t handle) {
    shm_t* s = (shm_t*)(unsigned long)handle;
    return s->size;
}

int64_t aria_libc_shm_read_int64(int64_t handle, int64_t offset) {
    shm_t* s = (shm_t*)(unsigned long)handle;
    int64_t val;
    memcpy(&val, (char*)s->ptr + offset, sizeof(int64_t));
    return val;
}

void aria_libc_shm_write_int64(int64_t handle, int64_t offset, int64_t value) {
    shm_t* s = (shm_t*)(unsigned long)handle;
    memcpy((char*)s->ptr + offset, &value, sizeof(int64_t));
}
