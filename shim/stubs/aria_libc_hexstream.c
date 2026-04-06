// aria-libc hexstream shim — FD 3-5 I/O for AriaX kernel
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>

#define HEXSTREAM_FD3 3
#define HEXSTREAM_FD4 4
#define HEXSTREAM_FD5 5

static int hs_initialized = 0;
static int hs_fds[3] = {-1, -1, -1};

int aria_libc_hexstream_init(void) {
    hs_fds[0] = HEXSTREAM_FD3;
    hs_fds[1] = HEXSTREAM_FD4;
    hs_fds[2] = HEXSTREAM_FD5;
    hs_initialized = 1;
    return 0;
}

int64_t aria_libc_hexstream_fd(int64_t index) {
    if (index < 0 || index > 2) return -1;
    return (int64_t)hs_fds[index];
}

int aria_libc_hexstream_is_open(int64_t index) {
    if (index < 0 || index > 2) return 0;
    return fcntl(hs_fds[index], F_GETFD) != -1;
}

int aria_libc_hexstream_write(int64_t index, const char* data) {
    if (index < 0 || index > 2 || !data) return -1;
    size_t len = strlen(data);
    return (int)write(hs_fds[index], data, len);
}

int aria_libc_hexstream_write_int64(int64_t index, int64_t value) {
    if (index < 0 || index > 2) return -1;
    return (int)write(hs_fds[index], &value, sizeof(value));
}

int64_t aria_libc_hexstream_read_int64(int64_t index) {
    if (index < 0 || index > 2) return -1;
    int64_t val = 0;
    if (read(hs_fds[index], &val, sizeof(val)) != sizeof(val)) return -1;
    return val;
}

const char* aria_libc_hexstream_read_line(int64_t index) {
    static char buf[4096];
    if (index < 0 || index > 2) return "";
    ssize_t n = read(hs_fds[index], buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    buf[n] = '\0';
    return buf;
}

int aria_libc_hexstream_redirect_to_file(int64_t index, const char* path) {
    if (index < 0 || index > 2) return -1;
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    dup2(fd, hs_fds[index]);
    close(fd);
    return 0;
}

int aria_libc_hexstream_redirect_from_file(int64_t index, const char* path) {
    if (index < 0 || index > 2) return -1;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    dup2(fd, hs_fds[index]);
    close(fd);
    return 0;
}

int aria_libc_hexstream_debug(const char* msg) {
    return (int)write(STDERR_FILENO, msg, strlen(msg));
}
