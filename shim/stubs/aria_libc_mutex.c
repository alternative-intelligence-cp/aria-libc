// aria-libc mutex shim — pthreads wrapper
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

int64_t aria_libc_mutex_create(void) {
    pthread_mutex_t* m = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if (!m) return 0;
    if (pthread_mutex_init(m, NULL) != 0) {
        free(m);
        return 0;
    }
    return (int64_t)(unsigned long)m;
}

int aria_libc_mutex_lock(int64_t handle) {
    return pthread_mutex_lock((pthread_mutex_t*)(unsigned long)handle);
}

int aria_libc_mutex_unlock(int64_t handle) {
    return pthread_mutex_unlock((pthread_mutex_t*)(unsigned long)handle);
}

int aria_libc_mutex_trylock(int64_t handle) {
    return pthread_mutex_trylock((pthread_mutex_t*)(unsigned long)handle);
}

int aria_libc_mutex_destroy(int64_t handle) {
    pthread_mutex_t* m = (pthread_mutex_t*)(unsigned long)handle;
    int r = pthread_mutex_destroy(m);
    free(m);
    return r;
}
