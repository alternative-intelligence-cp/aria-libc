// aria-libc channel shim — basic bounded channel using pthreads
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_CHAN_BUF 1024

typedef struct {
    int64_t* buffer;
    int64_t capacity;
    int64_t count;
    int64_t head;
    int64_t tail;
    int closed;
    int mode; // 0=buffered, 1=unbuffered, 2=oneshot
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} channel_t;

int64_t aria_libc_channel_create(int64_t capacity) {
    channel_t* c = (channel_t*)calloc(1, sizeof(channel_t));
    if (!c) return 0;
    c->capacity = capacity > 0 ? capacity : 1;
    c->buffer = (int64_t*)calloc((size_t)c->capacity, sizeof(int64_t));
    if (!c->buffer) { free(c); return 0; }
    c->mode = 0;
    pthread_mutex_init(&c->lock, NULL);
    pthread_cond_init(&c->not_empty, NULL);
    pthread_cond_init(&c->not_full, NULL);
    return (int64_t)(unsigned long)c;
}

int64_t aria_libc_channel_create_unbuffered(void) {
    int64_t h = aria_libc_channel_create(1);
    if (h) ((channel_t*)(unsigned long)h)->mode = 1;
    return h;
}

int64_t aria_libc_channel_create_oneshot(void) {
    int64_t h = aria_libc_channel_create(1);
    if (h) ((channel_t*)(unsigned long)h)->mode = 2;
    return h;
}

int aria_libc_channel_send(int64_t handle, int64_t value) {
    channel_t* c = (channel_t*)(unsigned long)handle;
    pthread_mutex_lock(&c->lock);
    while (c->count >= c->capacity && !c->closed) {
        pthread_cond_wait(&c->not_full, &c->lock);
    }
    if (c->closed) { pthread_mutex_unlock(&c->lock); return -1; }
    c->buffer[c->tail] = value;
    c->tail = (c->tail + 1) % c->capacity;
    c->count++;
    pthread_cond_signal(&c->not_empty);
    pthread_mutex_unlock(&c->lock);
    return 0;
}

int64_t aria_libc_channel_recv(int64_t handle) {
    channel_t* c = (channel_t*)(unsigned long)handle;
    pthread_mutex_lock(&c->lock);
    while (c->count == 0 && !c->closed) {
        pthread_cond_wait(&c->not_empty, &c->lock);
    }
    if (c->count == 0) { pthread_mutex_unlock(&c->lock); return -1; }
    int64_t val = c->buffer[c->head];
    c->head = (c->head + 1) % c->capacity;
    c->count--;
    pthread_cond_signal(&c->not_full);
    pthread_mutex_unlock(&c->lock);
    return val;
}

int aria_libc_channel_try_send(int64_t handle, int64_t value) {
    channel_t* c = (channel_t*)(unsigned long)handle;
    pthread_mutex_lock(&c->lock);
    if (c->closed || c->count >= c->capacity) {
        pthread_mutex_unlock(&c->lock);
        return -1;
    }
    c->buffer[c->tail] = value;
    c->tail = (c->tail + 1) % c->capacity;
    c->count++;
    pthread_cond_signal(&c->not_empty);
    pthread_mutex_unlock(&c->lock);
    return 0;
}

int64_t aria_libc_channel_try_recv(int64_t handle) {
    channel_t* c = (channel_t*)(unsigned long)handle;
    pthread_mutex_lock(&c->lock);
    if (c->count == 0) {
        pthread_mutex_unlock(&c->lock);
        return -1;
    }
    int64_t val = c->buffer[c->head];
    c->head = (c->head + 1) % c->capacity;
    c->count--;
    pthread_cond_signal(&c->not_full);
    pthread_mutex_unlock(&c->lock);
    return val;
}

void aria_libc_channel_close(int64_t handle) {
    channel_t* c = (channel_t*)(unsigned long)handle;
    pthread_mutex_lock(&c->lock);
    c->closed = 1;
    pthread_cond_broadcast(&c->not_empty);
    pthread_cond_broadcast(&c->not_full);
    pthread_mutex_unlock(&c->lock);
}

void aria_libc_channel_destroy(int64_t handle) {
    channel_t* c = (channel_t*)(unsigned long)handle;
    pthread_mutex_destroy(&c->lock);
    pthread_cond_destroy(&c->not_empty);
    pthread_cond_destroy(&c->not_full);
    free(c->buffer);
    free(c);
}

int64_t aria_libc_channel_count(int64_t handle) {
    channel_t* c = (channel_t*)(unsigned long)handle;
    pthread_mutex_lock(&c->lock);
    int64_t n = c->count;
    pthread_mutex_unlock(&c->lock);
    return n;
}

int64_t aria_libc_channel_capacity(int64_t handle) {
    channel_t* c = (channel_t*)(unsigned long)handle;
    return c->capacity;
}

int aria_libc_channel_is_closed(int64_t handle) {
    channel_t* c = (channel_t*)(unsigned long)handle;
    return c->closed;
}

int aria_libc_channel_get_mode(int64_t handle) {
    channel_t* c = (channel_t*)(unsigned long)handle;
    return c->mode;
}

// Select: poll multiple channels, return index of first ready one
int aria_libc_channel_select2(int64_t h1, int64_t h2) {
    // Simple spin-poll
    for (int i = 0; i < 10000; i++) {
        channel_t* c1 = (channel_t*)(unsigned long)h1;
        channel_t* c2 = (channel_t*)(unsigned long)h2;
        if (c1->count > 0) return 0;
        if (c2->count > 0) return 1;
        sched_yield();
    }
    return -1;
}

int aria_libc_channel_select3(int64_t h1, int64_t h2, int64_t h3) {
    for (int i = 0; i < 10000; i++) {
        if (((channel_t*)(unsigned long)h1)->count > 0) return 0;
        if (((channel_t*)(unsigned long)h2)->count > 0) return 1;
        if (((channel_t*)(unsigned long)h3)->count > 0) return 2;
        sched_yield();
    }
    return -1;
}

int aria_libc_channel_select4(int64_t h1, int64_t h2, int64_t h3, int64_t h4) {
    for (int i = 0; i < 10000; i++) {
        if (((channel_t*)(unsigned long)h1)->count > 0) return 0;
        if (((channel_t*)(unsigned long)h2)->count > 0) return 1;
        if (((channel_t*)(unsigned long)h3)->count > 0) return 2;
        if (((channel_t*)(unsigned long)h4)->count > 0) return 3;
        sched_yield();
    }
    return -1;
}
