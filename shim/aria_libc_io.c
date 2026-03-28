/*  aria_libc_io.c — Flat-parameter C bridge between Aria FFI and libc I/O functions
 *
 *  Build (dynamic): cc -O2 -shared -fPIC -Wall -o libaria_libc_io.so aria_libc_io.c
 *  Build (static):  cc -O2 -c -Wall -o aria_libc_io.o aria_libc_io.c
 *                   ar rcs libaria_libc_io.a aria_libc_io.o
 *
 *  Aria ABI rules:
 *    - String params  = const char* (null-terminated)
 *    - String returns = AriaString {char* data, int64_t length} by value
 *    - flt32/flt64    = double at C ABI
 *    - "free" in names = shadowed — use _release/_destroy/_cleanup
 */

#define _DEFAULT_SOURCE   /* for DT_DIR, DT_REG, etc. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
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

int64_t aria_libc_io_errno(void) {
    return (int64_t)last_errno;
}

AriaString aria_libc_io_strerror(int64_t errnum) {
    return make_aria_string(strerror((int)errnum));
}

/* ── File descriptor operations ──────────────────────────────────────── */

/* Flags exposed as constants (matches POSIX):
 *   O_RDONLY = 0, O_WRONLY = 1, O_RDWR = 2
 *   O_CREAT = 64, O_TRUNC = 512, O_APPEND = 1024
 *   Combine with bitwise OR in Aria via uint32 ops
 */

int64_t aria_libc_io_open(const char *path, int64_t flags, int64_t mode) {
    int fd = open(path, (int)flags, (mode_t)mode);
    if (fd < 0) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return (int64_t)fd;
}

int64_t aria_libc_io_close(int64_t fd) {
    int result = close((int)fd);
    if (result < 0) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return 0;
}

int64_t aria_libc_io_read(int64_t fd, int64_t buf_id, int64_t count);
/* Forward-declared — we need a buffer pool for read. See below. */

int64_t aria_libc_io_write_string(int64_t fd, const char *data) {
    if (!data) {
        last_errno = EINVAL;
        return -1;
    }
    ssize_t len = (ssize_t)strlen(data);
    ssize_t written = write((int)fd, data, (size_t)len);
    if (written < 0) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return (int64_t)written;
}

int64_t aria_libc_io_write_bytes(int64_t fd, int64_t buf_id, int64_t count);
/* Forward-declared — needs buffer pool */

int64_t aria_libc_io_seek(int64_t fd, int64_t offset, int64_t whence) {
    /* whence: 0=SEEK_SET, 1=SEEK_CUR, 2=SEEK_END */
    off_t result = lseek((int)fd, (off_t)offset, (int)whence);
    if (result < 0) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return (int64_t)result;
}

/* ── Read buffer pool ────────────────────────────────────────────────── */
/* Aria can't directly manage C pointers for read buffers, so we provide
 * a simple pool of reusable buffers accessible by integer ID. */

#define MAX_BUFFERS  32
#define DEFAULT_BUF  8192

typedef struct {
    char    *data;
    int64_t  capacity;
    int64_t  used;         /* bytes from last read */
    int      active;
} ReadBuffer;

static ReadBuffer buffers[MAX_BUFFERS];

int64_t aria_libc_io_buf_create(int64_t capacity) {
    if (capacity <= 0) capacity = DEFAULT_BUF;
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (!buffers[i].active) {
            buffers[i].data = (char *)malloc((size_t)capacity);
            if (!buffers[i].data) {
                last_errno = ENOMEM;
                return -1;
            }
            buffers[i].capacity = capacity;
            buffers[i].used = 0;
            buffers[i].active = 1;
            last_errno = 0;
            return (int64_t)i;
        }
    }
    last_errno = ENOMEM;
    return -1;
}

int64_t aria_libc_io_buf_release(int64_t buf_id) {
    if (buf_id < 0 || buf_id >= MAX_BUFFERS || !buffers[buf_id].active) {
        last_errno = EINVAL;
        return -1;
    }
    free(buffers[buf_id].data);
    buffers[buf_id].data = NULL;
    buffers[buf_id].capacity = 0;
    buffers[buf_id].used = 0;
    buffers[buf_id].active = 0;
    last_errno = 0;
    return 0;
}

int64_t aria_libc_io_buf_size(int64_t buf_id) {
    if (buf_id < 0 || buf_id >= MAX_BUFFERS || !buffers[buf_id].active) {
        last_errno = EINVAL;
        return -1;
    }
    return buffers[buf_id].used;
}

AriaString aria_libc_io_buf_to_string(int64_t buf_id) {
    if (buf_id < 0 || buf_id >= MAX_BUFFERS || !buffers[buf_id].active) {
        last_errno = EINVAL;
        return make_aria_string("");
    }
    /* Copy buffer contents to a new null-terminated string */
    int64_t len = buffers[buf_id].used;
    char *copy = (char *)malloc((size_t)(len + 1));
    if (!copy) {
        last_errno = ENOMEM;
        return make_aria_string("");
    }
    memcpy(copy, buffers[buf_id].data, (size_t)len);
    copy[len] = '\0';
    AriaString r = { copy, len };
    last_errno = 0;
    return r;
}

/* Now implement read/write with buffer IDs */

int64_t aria_libc_io_read(int64_t fd, int64_t buf_id, int64_t count) {
    if (buf_id < 0 || buf_id >= MAX_BUFFERS || !buffers[buf_id].active) {
        last_errno = EINVAL;
        return -1;
    }
    if (count > buffers[buf_id].capacity) count = buffers[buf_id].capacity;
    ssize_t n = read((int)fd, buffers[buf_id].data, (size_t)count);
    if (n < 0) {
        last_errno = errno;
        buffers[buf_id].used = 0;
        return -1;
    }
    buffers[buf_id].used = (int64_t)n;
    last_errno = 0;
    return (int64_t)n;
}

int64_t aria_libc_io_write_bytes(int64_t fd, int64_t buf_id, int64_t count) {
    if (buf_id < 0 || buf_id >= MAX_BUFFERS || !buffers[buf_id].active) {
        last_errno = EINVAL;
        return -1;
    }
    if (count > buffers[buf_id].used) count = buffers[buf_id].used;
    ssize_t written = write((int)fd, buffers[buf_id].data, (size_t)count);
    if (written < 0) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return (int64_t)written;
}

/* ── Stat ────────────────────────────────────────────────────────────── */

int64_t aria_libc_io_stat_size(const char *path) {
    struct stat st;
    if (stat(path, &st) < 0) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return (int64_t)st.st_size;
}

int64_t aria_libc_io_stat_mode(const char *path) {
    struct stat st;
    if (stat(path, &st) < 0) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return (int64_t)st.st_mode;
}

int64_t aria_libc_io_stat_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) < 0) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return (int64_t)st.st_mtime;
}

int64_t aria_libc_io_stat_is_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) < 0) {
        last_errno = errno;
        return 0;
    }
    last_errno = 0;
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

int64_t aria_libc_io_stat_is_file(const char *path) {
    struct stat st;
    if (stat(path, &st) < 0) {
        last_errno = errno;
        return 0;
    }
    last_errno = 0;
    return S_ISREG(st.st_mode) ? 1 : 0;
}

int64_t aria_libc_io_stat_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) < 0) {
        return 0;
    }
    return 1;
}

/* ── Directory operations ────────────────────────────────────────────── */

int64_t aria_libc_io_mkdir(const char *path, int64_t mode) {
    if (mkdir(path, (mode_t)mode) < 0) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return 0;
}

int64_t aria_libc_io_rmdir(const char *path) {
    if (rmdir(path) < 0) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return 0;
}

int64_t aria_libc_io_unlink(const char *path) {
    if (unlink(path) < 0) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return 0;
}

int64_t aria_libc_io_rename(const char *old_path, const char *new_path) {
    if (rename(old_path, new_path) < 0) {
        last_errno = errno;
        return -1;
    }
    last_errno = 0;
    return 0;
}

/* Directory listing — handle-based (same pool pattern as buffers) */

#define MAX_DIRS 16

static DIR *dir_handles[MAX_DIRS];

int64_t aria_libc_io_opendir(const char *path) {
    for (int i = 0; i < MAX_DIRS; i++) {
        if (!dir_handles[i]) {
            dir_handles[i] = opendir(path);
            if (!dir_handles[i]) {
                last_errno = errno;
                return -1;
            }
            last_errno = 0;
            return (int64_t)i;
        }
    }
    last_errno = ENOMEM;
    return -1;
}

AriaString aria_libc_io_readdir_next(int64_t dir_id) {
    if (dir_id < 0 || dir_id >= MAX_DIRS || !dir_handles[dir_id]) {
        last_errno = EINVAL;
        return make_aria_string("");
    }
    struct dirent *entry = readdir(dir_handles[dir_id]);
    if (!entry) {
        /* End of directory — not an error, just empty string */
        last_errno = 0;
        return make_aria_string("");
    }
    last_errno = 0;
    return make_aria_string(entry->d_name);
}

int64_t aria_libc_io_readdir_type(int64_t dir_id) {
    /* Returns type of LAST entry read: 1=file, 2=dir, 0=other/unknown */
    /* Must be called right after readdir_next */
    (void)dir_id;
    /* We can't get the type of the last entry without re-reading.
     * Instead, callers should use stat_is_dir/stat_is_file.
     * This is a placeholder that returns 0 (unknown). */
    return 0;
}

int64_t aria_libc_io_closedir(int64_t dir_id) {
    if (dir_id < 0 || dir_id >= MAX_DIRS || !dir_handles[dir_id]) {
        last_errno = EINVAL;
        return -1;
    }
    closedir(dir_handles[dir_id]);
    dir_handles[dir_id] = NULL;
    last_errno = 0;
    return 0;
}

/* ── Convenience: read entire file to string ─────────────────────────── */

AriaString aria_libc_io_read_file(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        last_errno = errno;
        return make_aria_string("");
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        last_errno = errno;
        close(fd);
        return make_aria_string("");
    }

    int64_t size = (int64_t)st.st_size;
    char *buf = (char *)malloc((size_t)(size + 1));
    if (!buf) {
        last_errno = ENOMEM;
        close(fd);
        return make_aria_string("");
    }

    ssize_t n = read(fd, buf, (size_t)size);
    close(fd);

    if (n < 0) {
        last_errno = errno;
        free(buf);
        return make_aria_string("");
    }

    buf[n] = '\0';
    AriaString r = { buf, (int64_t)n };
    last_errno = 0;
    return r;
}

int64_t aria_libc_io_write_file(const char *path, const char *content) {
    if (!content) {
        last_errno = EINVAL;
        return -1;
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        last_errno = errno;
        return -1;
    }

    size_t len = strlen(content);
    ssize_t written = write(fd, content, len);
    close(fd);

    if (written < 0) {
        last_errno = errno;
        return -1;
    }

    last_errno = 0;
    return (int64_t)written;
}

int64_t aria_libc_io_append_file(const char *path, const char *content) {
    if (!content) {
        last_errno = EINVAL;
        return -1;
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        last_errno = errno;
        return -1;
    }

    size_t len = strlen(content);
    ssize_t written = write(fd, content, len);
    close(fd);

    if (written < 0) {
        last_errno = errno;
        return -1;
    }

    last_errno = 0;
    return (int64_t)written;
}

/* ── POSIX flag constants (exposed as functions for Aria access) ────── */

int64_t aria_libc_io_O_RDONLY(void)  { return O_RDONLY; }
int64_t aria_libc_io_O_WRONLY(void)  { return O_WRONLY; }
int64_t aria_libc_io_O_RDWR(void)    { return O_RDWR; }
int64_t aria_libc_io_O_CREAT(void)   { return O_CREAT; }
int64_t aria_libc_io_O_TRUNC(void)   { return O_TRUNC; }
int64_t aria_libc_io_O_APPEND(void)  { return O_APPEND; }
int64_t aria_libc_io_SEEK_SET(void)  { return SEEK_SET; }
int64_t aria_libc_io_SEEK_CUR(void)  { return SEEK_CUR; }
int64_t aria_libc_io_SEEK_END(void)  { return SEEK_END; }
