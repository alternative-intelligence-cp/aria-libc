// aria-libc thread pool shim
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>

#define MAX_TASKS 4096

typedef struct {
    int64_t (*fn)(void*);
    void* arg;
} task_t;

typedef struct {
    pthread_t* workers;
    int64_t num_workers;
    task_t tasks[MAX_TASKS];
    int64_t task_head;
    int64_t task_tail;
    atomic_int_least64_t pending;
    atomic_int_least64_t active;
    int shutdown;
    pthread_mutex_t lock;
    pthread_cond_t task_avail;
    pthread_cond_t idle_cond;
} pool_t;

static void* pool_worker(void* arg) {
    pool_t* p = (pool_t*)arg;
    while (1) {
        pthread_mutex_lock(&p->lock);
        while (p->task_head == p->task_tail && !p->shutdown) {
            pthread_cond_wait(&p->task_avail, &p->lock);
        }
        if (p->shutdown && p->task_head == p->task_tail) {
            pthread_mutex_unlock(&p->lock);
            break;
        }
        task_t t = p->tasks[p->task_head % MAX_TASKS];
        p->task_head++;
        atomic_fetch_add(&p->active, 1);
        pthread_mutex_unlock(&p->lock);

        t.fn(t.arg);

        atomic_fetch_sub(&p->active, 1);
        atomic_fetch_sub(&p->pending, 1);
        pthread_cond_broadcast(&p->idle_cond);
    }
    return NULL;
}

int64_t aria_libc_pool_create(int64_t num_workers) {
    pool_t* p = (pool_t*)calloc(1, sizeof(pool_t));
    if (!p) return 0;
    p->num_workers = num_workers;
    p->workers = (pthread_t*)calloc((size_t)num_workers, sizeof(pthread_t));
    pthread_mutex_init(&p->lock, NULL);
    pthread_cond_init(&p->task_avail, NULL);
    pthread_cond_init(&p->idle_cond, NULL);
    for (int64_t i = 0; i < num_workers; i++) {
        pthread_create(&p->workers[i], NULL, pool_worker, p);
    }
    return (int64_t)(unsigned long)p;
}

int aria_libc_pool_submit(int64_t handle, int64_t fn_ptr, int64_t arg_ptr) {
    pool_t* p = (pool_t*)(unsigned long)handle;
    pthread_mutex_lock(&p->lock);
    int64_t idx = p->task_tail % MAX_TASKS;
    p->tasks[idx].fn = (int64_t(*)(void*))(unsigned long)fn_ptr;
    p->tasks[idx].arg = (void*)(unsigned long)arg_ptr;
    p->task_tail++;
    atomic_fetch_add(&p->pending, 1);
    pthread_cond_signal(&p->task_avail);
    pthread_mutex_unlock(&p->lock);
    return 0;
}

void aria_libc_pool_wait_idle(int64_t handle) {
    pool_t* p = (pool_t*)(unsigned long)handle;
    pthread_mutex_lock(&p->lock);
    while (atomic_load(&p->pending) > 0 || atomic_load(&p->active) > 0) {
        pthread_cond_wait(&p->idle_cond, &p->lock);
    }
    pthread_mutex_unlock(&p->lock);
}

void aria_libc_pool_shutdown(int64_t handle) {
    pool_t* p = (pool_t*)(unsigned long)handle;
    pthread_mutex_lock(&p->lock);
    p->shutdown = 1;
    pthread_cond_broadcast(&p->task_avail);
    pthread_mutex_unlock(&p->lock);
    for (int64_t i = 0; i < p->num_workers; i++) {
        pthread_join(p->workers[i], NULL);
    }
    pthread_mutex_destroy(&p->lock);
    pthread_cond_destroy(&p->task_avail);
    pthread_cond_destroy(&p->idle_cond);
    free(p->workers);
    free(p);
}

int64_t aria_libc_pool_worker_count(int64_t handle) {
    pool_t* p = (pool_t*)(unsigned long)handle;
    return p->num_workers;
}

int64_t aria_libc_pool_pending_tasks(int64_t handle) {
    pool_t* p = (pool_t*)(unsigned long)handle;
    return (int64_t)atomic_load(&p->pending);
}

int64_t aria_libc_pool_active_tasks(int64_t handle) {
    pool_t* p = (pool_t*)(unsigned long)handle;
    return (int64_t)atomic_load(&p->active);
}
