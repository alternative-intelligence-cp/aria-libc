// aria-libc rwlock shim — pthreads rwlock wrapper
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

int64_t aria_libc_rwlock_create(void) {
    pthread_rwlock_t* rw = (pthread_rwlock_t*)malloc(sizeof(pthread_rwlock_t));
    if (!rw) return 0;
    if (pthread_rwlock_init(rw, NULL) != 0) {
        free(rw);
        return 0;
    }
    return (int64_t)(unsigned long)rw;
}

int aria_libc_rwlock_rdlock(int64_t handle) {
    return pthread_rwlock_rdlock((pthread_rwlock_t*)(unsigned long)handle);
}

int aria_libc_rwlock_wrlock(int64_t handle) {
    return pthread_rwlock_wrlock((pthread_rwlock_t*)(unsigned long)handle);
}

int aria_libc_rwlock_unlock(int64_t handle) {
    return pthread_rwlock_unlock((pthread_rwlock_t*)(unsigned long)handle);
}

int aria_libc_rwlock_destroy(int64_t handle) {
    pthread_rwlock_t* rw = (pthread_rwlock_t*)(unsigned long)handle;
    int ret = pthread_rwlock_destroy(rw);
    free(rw);
    return ret;
}
