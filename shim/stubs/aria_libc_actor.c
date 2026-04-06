// aria-libc actor shim — simple message-passing actor model
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>

#define MAILBOX_SIZE 256

typedef struct {
    int64_t messages[MAILBOX_SIZE];
    int64_t head;
    int64_t tail;
    int64_t count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_t thread;
    int64_t (*handler)(int64_t);
    atomic_int alive;
    int64_t reply_channel; // for request-reply pattern
} actor_t;

static __thread int64_t tls_reply_channel = 0;

static void* actor_loop(void* arg) {
    actor_t* a = (actor_t*)arg;
    while (atomic_load(&a->alive)) {
        pthread_mutex_lock(&a->lock);
        while (a->count == 0 && atomic_load(&a->alive)) {
            pthread_cond_wait(&a->not_empty, &a->lock);
        }
        if (!atomic_load(&a->alive) && a->count == 0) {
            pthread_mutex_unlock(&a->lock);
            break;
        }
        int64_t msg = a->messages[a->head % MAILBOX_SIZE];
        a->head++;
        a->count--;
        pthread_mutex_unlock(&a->lock);
        if (a->handler) a->handler(msg);
    }
    return NULL;
}

int64_t aria_libc_actor_spawn(int64_t handler_ptr) {
    actor_t* a = (actor_t*)calloc(1, sizeof(actor_t));
    if (!a) return 0;
    a->handler = (int64_t(*)(int64_t))(unsigned long)handler_ptr;
    atomic_store(&a->alive, 1);
    pthread_mutex_init(&a->lock, NULL);
    pthread_cond_init(&a->not_empty, NULL);
    pthread_create(&a->thread, NULL, actor_loop, a);
    return (int64_t)(unsigned long)a;
}

int aria_libc_actor_send(int64_t handle, int64_t message) {
    actor_t* a = (actor_t*)(unsigned long)handle;
    pthread_mutex_lock(&a->lock);
    if (a->count >= MAILBOX_SIZE) {
        pthread_mutex_unlock(&a->lock);
        return -1; // mailbox full
    }
    a->messages[(a->tail) % MAILBOX_SIZE] = message;
    a->tail++;
    a->count++;
    pthread_cond_signal(&a->not_empty);
    pthread_mutex_unlock(&a->lock);
    return 0;
}

int aria_libc_actor_try_send(int64_t handle, int64_t message) {
    return aria_libc_actor_send(handle, message);
}

void aria_libc_actor_stop(int64_t handle) {
    actor_t* a = (actor_t*)(unsigned long)handle;
    atomic_store(&a->alive, 0);
    pthread_cond_broadcast(&a->not_empty);
    pthread_join(a->thread, NULL);
}

void aria_libc_actor_destroy(int64_t handle) {
    actor_t* a = (actor_t*)(unsigned long)handle;
    if (atomic_load(&a->alive)) {
        aria_libc_actor_stop(handle);
    }
    pthread_mutex_destroy(&a->lock);
    pthread_cond_destroy(&a->not_empty);
    free(a);
}

int aria_libc_actor_is_alive(int64_t handle) {
    actor_t* a = (actor_t*)(unsigned long)handle;
    return atomic_load(&a->alive);
}

int64_t aria_libc_actor_mailbox(int64_t handle) {
    actor_t* a = (actor_t*)(unsigned long)handle;
    pthread_mutex_lock(&a->lock);
    int64_t n = a->count;
    pthread_mutex_unlock(&a->lock);
    return n;
}

int64_t aria_libc_actor_pending(int64_t handle) {
    return aria_libc_actor_mailbox(handle);
}

// Reply channel support
int64_t aria_libc_reply(int64_t value) {
    // Send reply back via thread-local reply channel
    if (tls_reply_channel) {
        // For now, just store the value
        return value;
    }
    return -1;
}

int64_t aria_libc_get_reply_channel(void) {
    return tls_reply_channel;
}

void aria_libc_set_reply_channel(int64_t ch) {
    tls_reply_channel = ch;
}
