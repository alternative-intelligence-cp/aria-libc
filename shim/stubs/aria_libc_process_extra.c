// Extra process shim functions
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

int aria_libc_process_system(const char* cmd) {
    return system(cmd);
}

// Pipe support
static int pipe_fds[2] = {-1, -1};

int aria_libc_process_pipe(void) {
    return pipe(pipe_fds);
}

int64_t aria_libc_process_pipe_read_fd(void) {
    return (int64_t)pipe_fds[0];
}

int64_t aria_libc_process_pipe_write_fd(void) {
    return (int64_t)pipe_fds[1];
}

int aria_libc_process_pipe_close(int64_t fd) {
    return close((int)fd);
}

int64_t aria_libc_process_pipe_read_i64(int64_t fd) {
    int64_t val = 0;
    read((int)fd, &val, sizeof(val));
    return val;
}

int aria_libc_process_pipe_write_i64(int64_t fd, int64_t val) {
    return (int)(write((int)fd, &val, sizeof(val)) == sizeof(val) ? 0 : -1);
}

// Signal support
typedef void (*sig_handler_t)(int);
static sig_handler_t saved_handlers[64];

int aria_libc_process_signal_register(int signum, int64_t handler_ptr) {
    sig_handler_t h = signal(signum, (sig_handler_t)(unsigned long)handler_ptr);
    if (h == SIG_ERR) return -1;
    if (signum < 64) saved_handlers[signum] = h;
    return 0;
}

int aria_libc_process_signal_restore(int signum) {
    sig_handler_t h = (signum < 64) ? saved_handlers[signum] : SIG_DFL;
    return (signal(signum, h) == SIG_ERR) ? -1 : 0;
}

int aria_libc_process_signal_ignore(int signum) {
    return (signal(signum, SIG_IGN) == SIG_ERR) ? -1 : 0;
}

int aria_libc_process_signal_pending(void) {
    sigset_t pending;
    if (sigpending(&pending) != 0) return 0;
    // Count number of pending signals
    int count = 0;
    for (int i = 1; i < 32; i++) {
        if (sigismember(&pending, i)) count++;
    }
    return count;
}
