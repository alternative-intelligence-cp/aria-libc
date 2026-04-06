// aria-libc thread shim — pthreads wrapper
#define _GNU_SOURCE
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    pthread_t handle;
    int64_t (*fn)(void*);
    void* arg;
} thread_ctx;

static void* thread_trampoline(void* arg) {
    thread_ctx* ctx = (thread_ctx*)arg;
    int64_t (*fn)(void*) = ctx->fn;
    void* user_arg = ctx->arg;
    int64_t result = fn(user_arg);
    return (void*)(unsigned long)result;
}

int64_t aria_libc_thread_spawn(int64_t fn_ptr, int64_t arg_ptr) {
    thread_ctx* ctx = (thread_ctx*)malloc(sizeof(thread_ctx));
    if (!ctx) return 0;
    ctx->fn = (int64_t(*)(void*))(unsigned long)fn_ptr;
    ctx->arg = (void*)(unsigned long)arg_ptr;
    if (pthread_create(&ctx->handle, NULL, thread_trampoline, ctx) != 0) {
        free(ctx);
        return 0;
    }
    return (int64_t)(unsigned long)ctx;
}

int64_t aria_libc_thread_join(int64_t handle) {
    thread_ctx* ctx = (thread_ctx*)(unsigned long)handle;
    void* retval = NULL;
    int r = pthread_join(ctx->handle, &retval);
    int64_t result = (int64_t)(unsigned long)retval;
    free(ctx);
    return r == 0 ? result : -1;
}

int aria_libc_thread_detach(int64_t handle) {
    thread_ctx* ctx = (thread_ctx*)(unsigned long)handle;
    int r = pthread_detach(ctx->handle);
    free(ctx);
    return r;
}

void aria_libc_thread_yield(void) {
    sched_yield();
}

void aria_libc_thread_sleep_ns(int64_t ns) {
    struct timespec ts;
    ts.tv_sec = ns / 1000000000LL;
    ts.tv_nsec = ns % 1000000000LL;
    nanosleep(&ts, NULL);
}

void aria_libc_thread_sleep_ms(int64_t ms) {
    aria_libc_thread_sleep_ns(ms * 1000000LL);
}

int aria_libc_thread_set_name(int64_t handle, const char* name) {
    thread_ctx* ctx = (thread_ctx*)(unsigned long)handle;
    return pthread_setname_np(ctx->handle, name);
}

int64_t aria_libc_thread_hardware_concurrency(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int64_t)n : 1;
}

int64_t aria_libc_thread_current_id(void) {
    return (int64_t)pthread_self();
}
