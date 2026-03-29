/*  aria_libc_posix.c — Extended POSIX wrappers: signals, fork/exec, pipe, mmap
 *
 *  Build (dynamic): cc -O2 -shared -fPIC -Wall -o libaria_libc_posix.so aria_libc_posix.c
 *  Build (static):  cc -O2 -c -Wall -o aria_libc_posix.o aria_libc_posix.c
 *                   ar rcs libaria_libc_posix.a aria_libc_posix.o
 *
 *  Aria ABI rules:
 *    - String params  = const char* (null-terminated)
 *    - String returns = AriaString {char* data, int64_t length} by value
 *    - All integers as int64_t
 *    - "free" in names = shadowed — use _release/_destroy/_cleanup
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>

/* ── Aria string return type ─────────────────────────────────────────── */

typedef struct {
    char    *data;
    int64_t  length;
} AriaString;

static AriaString make_aria_string(const char *s) {
    if (!s) { AriaString r = { NULL, 0 }; return r; }
    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (buf) { memcpy(buf, s, len + 1); }
    AriaString r = { buf, (int64_t)len };
    return r;
}

/* ── Error state ─────────────────────────────────────────────────────── */

static int last_errno = 0;

int64_t aria_libc_posix_errno(void) { return (int64_t)last_errno; }

AriaString aria_libc_posix_strerror(int64_t errnum) {
    return make_aria_string(strerror((int)errnum));
}

/* ════════════════════════════════════════════════════════════════════════
 *  SIGNAL CONSTANTS
 * ════════════════════════════════════════════════════════════════════════ */

int64_t aria_libc_posix_SIGHUP(void)  { return (int64_t)SIGHUP; }
int64_t aria_libc_posix_SIGINT(void)  { return (int64_t)SIGINT; }
int64_t aria_libc_posix_SIGQUIT(void) { return (int64_t)SIGQUIT; }
int64_t aria_libc_posix_SIGILL(void)  { return (int64_t)SIGILL; }
int64_t aria_libc_posix_SIGABRT(void) { return (int64_t)SIGABRT; }
int64_t aria_libc_posix_SIGFPE(void)  { return (int64_t)SIGFPE; }
int64_t aria_libc_posix_SIGKILL(void) { return (int64_t)SIGKILL; }
int64_t aria_libc_posix_SIGSEGV(void) { return (int64_t)SIGSEGV; }
int64_t aria_libc_posix_SIGPIPE(void) { return (int64_t)SIGPIPE; }
int64_t aria_libc_posix_SIGALRM(void) { return (int64_t)SIGALRM; }
int64_t aria_libc_posix_SIGTERM(void) { return (int64_t)SIGTERM; }
int64_t aria_libc_posix_SIGUSR1(void) { return (int64_t)SIGUSR1; }
int64_t aria_libc_posix_SIGUSR2(void) { return (int64_t)SIGUSR2; }
int64_t aria_libc_posix_SIGCHLD(void) { return (int64_t)SIGCHLD; }
int64_t aria_libc_posix_SIGCONT(void) { return (int64_t)SIGCONT; }
int64_t aria_libc_posix_SIGSTOP(void) { return (int64_t)SIGSTOP; }
int64_t aria_libc_posix_SIGTSTP(void) { return (int64_t)SIGTSTP; }

/* ════════════════════════════════════════════════════════════════════════
 *  SIGNAL HANDLING — flag-based (no Aria callbacks)
 *
 *  Model: trap a signal → handler sets a volatile flag → Aria code polls
 *  the flag with posix_signal_check(). This avoids needing Aria function
 *  pointers as C signal callbacks.
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX_SIGNAL 64
static volatile sig_atomic_t signal_flags[MAX_SIGNAL];

static void flag_handler(int sig) {
    if (sig >= 0 && sig < MAX_SIGNAL)
        signal_flags[sig] = 1;
}

/* Install flag-based handler for signum. Returns 0 on success, -1 on error. */
int64_t aria_libc_posix_signal_trap(int64_t signum) {
    int sig = (int)signum;
    if (sig < 0 || sig >= MAX_SIGNAL) { last_errno = EINVAL; return -1; }
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = flag_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(sig, &sa, NULL) < 0) { last_errno = errno; return -1; }
    signal_flags[sig] = 0;
    return 0;
}

/* Check if signum was received. Returns 1 and clears flag, 0 if not. */
int64_t aria_libc_posix_signal_check(int64_t signum) {
    int sig = (int)signum;
    if (sig < 0 || sig >= MAX_SIGNAL) return 0;
    if (signal_flags[sig]) {
        signal_flags[sig] = 0;
        return 1;
    }
    return 0;
}

/* Reset signal to default handler (SIG_DFL). */
int64_t aria_libc_posix_signal_reset(int64_t signum) {
    int sig = (int)signum;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    if (sigaction(sig, &sa, NULL) < 0) { last_errno = errno; return -1; }
    return 0;
}

/* Ignore a signal (SIG_IGN). */
int64_t aria_libc_posix_signal_ignore(int64_t signum) {
    int sig = (int)signum;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    if (sigaction(sig, &sa, NULL) < 0) { last_errno = errno; return -1; }
    return 0;
}

/* Send signal to self. */
int64_t aria_libc_posix_raise(int64_t signum) {
    int rc = raise((int)signum);
    if (rc != 0) { last_errno = errno; return -1; }
    return 0;
}

/* Send signal to another process. */
int64_t aria_libc_posix_kill(int64_t pid, int64_t signum) {
    int rc = kill((pid_t)pid, (int)signum);
    if (rc != 0) { last_errno = errno; return -1; }
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════
 *  FORK / EXEC / WAIT
 * ════════════════════════════════════════════════════════════════════════ */

/* Fork. Returns child PID in parent (>0), 0 in child, -1 on error. */
int64_t aria_libc_posix_fork(void) {
    pid_t p = fork();
    if (p < 0) { last_errno = errno; return -1; }
    return (int64_t)p;
}

/* Wait for specific child. flags = 0 for blocking, WNOHANG for non-blocking.
 * Returns: child pid on success, 0 if WNOHANG and child not done, -1 error.
 * Stores raw status internally for extraction with wait_exited/wait_status. */
static int last_wait_status = 0;

int64_t aria_libc_posix_waitpid(int64_t pid, int64_t flags) {
    int status = 0;
    pid_t rc = waitpid((pid_t)pid, &status, (int)flags);
    if (rc < 0) { last_errno = errno; return -1; }
    last_wait_status = status;
    return (int64_t)rc;
}

/* WNOHANG constant */
int64_t aria_libc_posix_WNOHANG(void) { return (int64_t)WNOHANG; }

/* Extract info from last waitpid status */
int64_t aria_libc_posix_wait_exited(void) {
    return WIFEXITED(last_wait_status) ? 1 : 0;
}

int64_t aria_libc_posix_wait_status(void) {
    return WIFEXITED(last_wait_status) ? (int64_t)WEXITSTATUS(last_wait_status) : -1;
}

int64_t aria_libc_posix_wait_signaled(void) {
    return WIFSIGNALED(last_wait_status) ? 1 : 0;
}

int64_t aria_libc_posix_wait_termsig(void) {
    return WIFSIGNALED(last_wait_status) ? (int64_t)WTERMSIG(last_wait_status) : -1;
}

/* Execute a shell command, replacing the current process.
 * This function only returns on error. */
int64_t aria_libc_posix_exec_shell(const char *cmd) {
    execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
    last_errno = errno;
    return -1; /* only reached on error */
}

/* Fork + exec + waitpid convenience. Returns exit status (0 = success). */
int64_t aria_libc_posix_spawn(const char *cmd) {
    pid_t p = fork();
    if (p < 0) { last_errno = errno; return -1; }
    if (p == 0) {
        /* child */
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127); /* exec failed */
    }
    /* parent */
    int status = 0;
    pid_t rc = waitpid(p, &status, 0);
    if (rc < 0) { last_errno = errno; return -1; }
    last_wait_status = status;
    return WIFEXITED(status) ? (int64_t)WEXITSTATUS(status) : -1;
}

/* Fork + exec without wait. Returns child PID. */
int64_t aria_libc_posix_spawn_bg(const char *cmd) {
    pid_t p = fork();
    if (p < 0) { last_errno = errno; return -1; }
    if (p == 0) {
        /* child */
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    return (int64_t)p;
}

/* Immediate process exit (no cleanup). */
void aria_libc_posix_exit_now(int64_t code) {
    _exit((int)code);
}

/* ════════════════════════════════════════════════════════════════════════
 *  PIPE + DUP
 * ════════════════════════════════════════════════════════════════════════ */

static int last_pipe_fds[2] = { -1, -1 };

/* Create a pipe. Returns 0 on success, -1 on error.
 * Use pipe_read_fd() and pipe_write_fd() to get the file descriptors. */
int64_t aria_libc_posix_pipe_create(void) {
    if (pipe(last_pipe_fds) < 0) { last_errno = errno; return -1; }
    return 0;
}

int64_t aria_libc_posix_pipe_read_fd(void) {
    return (int64_t)last_pipe_fds[0];
}

int64_t aria_libc_posix_pipe_write_fd(void) {
    return (int64_t)last_pipe_fds[1];
}

/* Duplicate a file descriptor. Returns new fd, -1 on error. */
int64_t aria_libc_posix_dup(int64_t oldfd) {
    int rc = dup((int)oldfd);
    if (rc < 0) { last_errno = errno; return -1; }
    return (int64_t)rc;
}

/* Duplicate fd to specific target. Returns newfd, -1 on error. */
int64_t aria_libc_posix_dup2(int64_t oldfd, int64_t newfd) {
    int rc = dup2((int)oldfd, (int)newfd);
    if (rc < 0) { last_errno = errno; return -1; }
    return (int64_t)rc;
}

/* Write a string to an fd. Returns bytes written, -1 on error. */
int64_t aria_libc_posix_fd_write_string(int64_t fd, const char *data) {
    if (!data) return 0;
    size_t len = strlen(data);
    ssize_t rc = write((int)fd, data, len);
    if (rc < 0) { last_errno = errno; return -1; }
    return (int64_t)rc;
}

/* Read up to max_bytes from an fd. Returns the data as a string. */
AriaString aria_libc_posix_fd_read_string(int64_t fd, int64_t max_bytes) {
    if (max_bytes <= 0) max_bytes = 4096;
    char *buf = (char *)malloc((size_t)max_bytes + 1);
    if (!buf) return make_aria_string("");
    ssize_t n = read((int)fd, buf, (size_t)max_bytes);
    if (n < 0) { last_errno = errno; free(buf); return make_aria_string(""); }
    buf[n] = '\0';
    AriaString r = { buf, (int64_t)n };
    return r;
}

/* Close an fd (generic — for pipe ends, duped fds, etc.). */
int64_t aria_libc_posix_fd_close(int64_t fd) {
    int rc = close((int)fd);
    if (rc < 0) { last_errno = errno; return -1; }
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════
 *  mmap CONSTANTS
 * ════════════════════════════════════════════════════════════════════════ */

int64_t aria_libc_posix_PROT_NONE(void)  { return (int64_t)PROT_NONE; }
int64_t aria_libc_posix_PROT_READ(void)  { return (int64_t)PROT_READ; }
int64_t aria_libc_posix_PROT_WRITE(void) { return (int64_t)PROT_WRITE; }
int64_t aria_libc_posix_PROT_EXEC(void)  { return (int64_t)PROT_EXEC; }

int64_t aria_libc_posix_MAP_SHARED(void)    { return (int64_t)MAP_SHARED; }
int64_t aria_libc_posix_MAP_PRIVATE(void)   { return (int64_t)MAP_PRIVATE; }
int64_t aria_libc_posix_MAP_ANONYMOUS(void) { return (int64_t)MAP_ANONYMOUS; }
int64_t aria_libc_posix_MAP_FIXED(void)     { return (int64_t)MAP_FIXED; }

int64_t aria_libc_posix_MS_ASYNC(void)      { return (int64_t)MS_ASYNC; }
int64_t aria_libc_posix_MS_SYNC(void)       { return (int64_t)MS_SYNC; }
int64_t aria_libc_posix_MS_INVALIDATE(void) { return (int64_t)MS_INVALIDATE; }

/* ════════════════════════════════════════════════════════════════════════
 *  mmap — Handle-based memory mapping
 *
 *  Aria code can't hold raw pointers safely, so we use a handle pool.
 *  Each mmap region gets an integer handle. Read/write through handles.
 * ════════════════════════════════════════════════════════════════════════ */

#define MMAP_POOL_SIZE 64

typedef struct {
    void   *addr;
    size_t  length;
    int     in_use;
} MmapEntry;

static MmapEntry mmap_pool[MMAP_POOL_SIZE];

static int mmap_pool_alloc(void *addr, size_t length) {
    for (int i = 0; i < MMAP_POOL_SIZE; i++) {
        if (!mmap_pool[i].in_use) {
            mmap_pool[i].addr    = addr;
            mmap_pool[i].length  = length;
            mmap_pool[i].in_use  = 1;
            return i;
        }
    }
    return -1;
}

static MmapEntry *mmap_pool_get(int handle) {
    if (handle < 0 || handle >= MMAP_POOL_SIZE) return NULL;
    if (!mmap_pool[handle].in_use) return NULL;
    return &mmap_pool[handle];
}

/* Map memory. Returns handle (>=0) or -1 on error.
 * fd = -1 for anonymous mapping. */
int64_t aria_libc_posix_mmap(int64_t length, int64_t prot, int64_t flags,
                              int64_t fd, int64_t offset) {
    void *addr = mmap(NULL, (size_t)length, (int)prot, (int)flags,
                      (int)fd, (off_t)offset);
    if (addr == MAP_FAILED) { last_errno = errno; return -1; }
    int handle = mmap_pool_alloc(addr, (size_t)length);
    if (handle < 0) {
        munmap(addr, (size_t)length);
        last_errno = ENOMEM;
        return -1;
    }
    return (int64_t)handle;
}

/* Unmap a region by handle. Returns 0 on success, -1 on error. */
int64_t aria_libc_posix_munmap(int64_t handle) {
    MmapEntry *e = mmap_pool_get((int)handle);
    if (!e) { last_errno = EINVAL; return -1; }
    int rc = munmap(e->addr, e->length);
    e->in_use = 0;
    e->addr = NULL;
    e->length = 0;
    if (rc < 0) { last_errno = errno; return -1; }
    return 0;
}

/* Change protection on a mapped region. */
int64_t aria_libc_posix_mprotect(int64_t handle, int64_t prot) {
    MmapEntry *e = mmap_pool_get((int)handle);
    if (!e) { last_errno = EINVAL; return -1; }
    int rc = mprotect(e->addr, e->length, (int)prot);
    if (rc < 0) { last_errno = errno; return -1; }
    return 0;
}

/* Sync a mapped region. flags = MS_SYNC or MS_ASYNC. */
int64_t aria_libc_posix_msync(int64_t handle, int64_t flags) {
    MmapEntry *e = mmap_pool_get((int)handle);
    if (!e) { last_errno = EINVAL; return -1; }
    int rc = msync(e->addr, e->length, (int)flags);
    if (rc < 0) { last_errno = errno; return -1; }
    return 0;
}

/* Get mapped region length. */
int64_t aria_libc_posix_mmap_length(int64_t handle) {
    MmapEntry *e = mmap_pool_get((int)handle);
    if (!e) return -1;
    return (int64_t)e->length;
}

/* Read a single byte at offset. Returns byte value (0-255) or -1 on error. */
int64_t aria_libc_posix_mmap_read_byte(int64_t handle, int64_t offset) {
    MmapEntry *e = mmap_pool_get((int)handle);
    if (!e) { last_errno = EINVAL; return -1; }
    if (offset < 0 || (size_t)offset >= e->length) { last_errno = EINVAL; return -1; }
    return (int64_t)((unsigned char *)e->addr)[offset];
}

/* Write a single byte at offset. Returns 0 on success, -1 on error. */
int64_t aria_libc_posix_mmap_write_byte(int64_t handle, int64_t offset, int64_t value) {
    MmapEntry *e = mmap_pool_get((int)handle);
    if (!e) { last_errno = EINVAL; return -1; }
    if (offset < 0 || (size_t)offset >= e->length) { last_errno = EINVAL; return -1; }
    ((unsigned char *)e->addr)[offset] = (unsigned char)(value & 0xFF);
    return 0;
}

/* Read an int64 at byte offset (must be 8-byte aligned within region). */
int64_t aria_libc_posix_mmap_read_int64(int64_t handle, int64_t offset) {
    MmapEntry *e = mmap_pool_get((int)handle);
    if (!e) { last_errno = EINVAL; return 0; }
    if (offset < 0 || (size_t)(offset + 8) > e->length) { last_errno = EINVAL; return 0; }
    int64_t val;
    memcpy(&val, (char *)e->addr + offset, sizeof(int64_t));
    return val;
}

/* Write an int64 at byte offset. */
int64_t aria_libc_posix_mmap_write_int64(int64_t handle, int64_t offset, int64_t value) {
    MmapEntry *e = mmap_pool_get((int)handle);
    if (!e) { last_errno = EINVAL; return -1; }
    if (offset < 0 || (size_t)(offset + 8) > e->length) { last_errno = EINVAL; return -1; }
    memcpy((char *)e->addr + offset, &value, sizeof(int64_t));
    return 0;
}

/* Read a string from mapped memory at offset for length bytes. */
AriaString aria_libc_posix_mmap_to_string(int64_t handle, int64_t offset, int64_t length) {
    MmapEntry *e = mmap_pool_get((int)handle);
    if (!e || offset < 0 || length < 0 || (size_t)(offset + length) > e->length) {
        return make_aria_string("");
    }
    char *buf = (char *)malloc((size_t)length + 1);
    if (!buf) return make_aria_string("");
    memcpy(buf, (char *)e->addr + offset, (size_t)length);
    buf[length] = '\0';
    AriaString r = { buf, length };
    return r;
}

/* Write a string into mapped memory at offset. Returns bytes written. */
int64_t aria_libc_posix_mmap_from_string(int64_t handle, int64_t offset, const char *data) {
    MmapEntry *e = mmap_pool_get((int)handle);
    if (!e || !data) { last_errno = EINVAL; return -1; }
    size_t len = strlen(data);
    if (offset < 0 || (size_t)offset + len > e->length) { last_errno = EINVAL; return -1; }
    memcpy((char *)e->addr + offset, data, len);
    return (int64_t)len;
}
