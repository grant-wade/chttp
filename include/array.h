/*
    array.h - Type-safe, thread-safe, cross-platform dynamic array (single-header C library)
    ----------------------------------------------------------------------------------------
    By default, all functions are type-safe, and you can define arrays for any type in one line.

    Compile with -lpthread (POSIX) if needed.

    Usage:
      #define ARRAY_IMPLEMENTATION
      #include "array.h"

      ARRAY_DECLARE(int, ints);
      ARRAY_DEFINE(int, ints);

      int main() {
          ints arr;
          ints(&arr, NULL);
          int v = 42; ints_push(&arr, v);
          printf("%d\n", ints_at(&arr, 0));
          ints_destroy(&arr);
      }
*/

#ifndef ARRAY_H
#define ARRAY_H
#include <string.h>

// --- Platform mutex abstraction ---
#if defined(_WIN32) || defined(_WIN64)
#define ARR_WIN32 1
#include <windows.h>
typedef SRWLOCK arr_mutex_t;
#define ARR_MUTEX_INIT(lock)               InitializeSRWLock(&(lock))
#define ARR_MUTEX_LOCK_WRITE(lock)         AcquireSRWLockExclusive(&(lock))
#define ARR_MUTEX_UNLOCK_WRITE(lock)       ReleaseSRWLockExclusive(&(lock))
#define ARR_MUTEX_LOCK_READ(lock)          AcquireSRWLockShared(&(lock))
#define ARR_MUTEX_UNLOCK_READ(lock)        ReleaseSRWLockShared(&(lock))
#else
#define ARR_PTHREAD 1
#include <pthread.h>
typedef pthread_mutex_t arr_mutex_t;
#define ARR_MUTEX_INIT(lock)               pthread_mutex_init(&(lock), NULL)
#define ARR_MUTEX_LOCK_WRITE(lock)         pthread_mutex_lock(&(lock))
#define ARR_MUTEX_UNLOCK_WRITE(lock)       pthread_mutex_unlock(&(lock))
#define ARR_MUTEX_LOCK_READ(lock)          pthread_mutex_lock((pthread_mutex_t*)&(lock))
#define ARR_MUTEX_UNLOCK_READ(lock)        pthread_mutex_unlock((pthread_mutex_t*)&(lock))
#endif

#define ARRAY_DECLARE(TYPE, NAME)                                                \
typedef struct {                                                                 \
    TYPE* data;                                                                  \
    size_t size, capacity;                                                       \
    arr_mutex_t mutex;                                                           \
    void* tag;                                                                   \
} NAME;                                                                          \
/* Added tag parameter to init */                                                \
bool NAME##_init(NAME* a, void* tag);                                            \
bool NAME##_init_size(NAME* a, size_t size, void* tag);                          \
void NAME##_destroy(NAME* a);                                                    \
bool NAME##_grow(NAME* a, size_t newcap);                                        \
bool NAME##_push(NAME* a, TYPE v);                                               \
bool NAME##_insert(NAME* a, size_t idx, TYPE v);                                 \
bool NAME##_pop(NAME* a, TYPE* out);                                             \
bool NAME##_remove(NAME* a, size_t idx, TYPE* out);                              \
bool NAME##_set(NAME* a, size_t idx, TYPE v);                                    \
bool NAME##_get(const NAME* a, size_t idx, TYPE* out);                           \
TYPE* NAME##_get_ptr(const NAME* a, size_t idx);                                 \
TYPE NAME##_head(NAME* a);                                                       \
TYPE NAME##_peek(NAME* a);                                                       \
bool NAME##_is_empty(NAME* a);                                                   \
TYPE NAME##_at(const NAME* a, size_t idx);                                       \
size_t NAME##_size(const NAME* a);                                               \
void NAME##_clear(NAME* a);

#define ARRAY_DEFINE(TYPE, NAME)                                                 \
/* Added tag parameter, store it */                                              \
bool NAME##_init(NAME* a, void* tag) {                                           \
    if (!a) return false;                                                        \
    a->data = NULL; a->size = 0; a->capacity = 0; a->tag = tag; /* Store tag */  \
    ARR_MUTEX_INIT(a->mutex);                                                    \
    return true;                                                                 \
}                                                                                \
bool NAME##_init_size(NAME* a, size_t size, void* tag) {                        \
    if (!a) return false;                                                        \
    a->data = NULL; a->size = 0; a->capacity = 0; a->tag = tag; /* Store tag */  \
    ARR_MUTEX_INIT(a->mutex);                                                    \
    if (size > 0) {                                                              \
        a->data = pmalloc(size * sizeof(TYPE), tag);                             \
        if (!a->data) return false;                                              \
        a->capacity = size;                                                      \
    }                                                                            \
    return true;                                                                 \
}                                                                                \
void NAME##_destroy(NAME* a) {                                                   \
    if (!a) return;                                                              \
    ARR_MUTEX_LOCK_WRITE(a->mutex);                                              \
    pfree(a->data); /* Use pfree */                                              \
    a->data=NULL; a->size=a->capacity=0;                                         \
    ARR_MUTEX_UNLOCK_WRITE(a->mutex);                                            \
}                                                                                \
bool NAME##_grow(NAME* a, size_t newcap) {                                       \
    if (!a) return false;                                                        \
    ARR_MUTEX_LOCK_WRITE(a->mutex);                                              \
    if (newcap > a->capacity) {                                                  \
        void* p = prealloc(a->data, newcap * sizeof(TYPE), a->tag);              \
        if (!p && newcap > 0) { /* Check p only if newcap > 0 */                 \
            ARR_MUTEX_UNLOCK_WRITE(a->mutex); return false;                      \
        }                                                                        \
        a->data = (TYPE*)p; a->capacity = newcap;                                \
    }                                                                            \
    ARR_MUTEX_UNLOCK_WRITE(a->mutex);                                            \
    return true;                                                                 \
}                                                                                \
bool NAME##_push(NAME* a, TYPE v) {                                              \
    if (!a) return false;                                                        \
    ARR_MUTEX_LOCK_WRITE(a->mutex);                                              \
    if (a->size == a->capacity) {                                                \
        size_t newcap = a->capacity ? a->capacity*2 : 8;                         \
        void* p = prealloc(a->data, newcap * sizeof(TYPE), a->tag);              \
        if (!p && newcap > 0) { /* Check p only if newcap > 0 */                 \
             ARR_MUTEX_UNLOCK_WRITE(a->mutex); return false;                     \
        }                                                                        \
        a->data = (TYPE*)p; a->capacity = newcap;                                \
    }                                                                            \
    a->data[a->size++] = v;                                                      \
    ARR_MUTEX_UNLOCK_WRITE(a->mutex);                                            \
    return true;                                                                 \
}                                                                                \
bool NAME##_insert(NAME* a, size_t idx, TYPE v) {                                \
    if (!a) return false;                                                        \
    ARR_MUTEX_LOCK_WRITE(a->mutex);                                              \
    if (idx > a->size) { ARR_MUTEX_UNLOCK_WRITE(a->mutex); return false; }       \
    if (a->size == a->capacity) {                                                \
        size_t nc = a->capacity ? a->capacity*2:8;                               \
        void* p = prealloc(a->data, nc*sizeof(TYPE), a->tag);                    \
        if (!p && nc > 0) { /* Check p only if nc > 0 */                         \
            ARR_MUTEX_UNLOCK_WRITE(a->mutex); return false;                      \
        }                                                                        \
        a->data = (TYPE*)p; a->capacity = nc;                                    \
    }                                                                            \
    memmove(&a->data[idx+1], &a->data[idx], (a->size-idx)*sizeof(TYPE));         \
    a->data[idx]=v; a->size++;                                                   \
    ARR_MUTEX_UNLOCK_WRITE(a->mutex);                                            \
    return true;                                                                 \
}                                                                                \
bool NAME##_pop(NAME* a, TYPE* out) {                                            \
    if (!a) return false;                                                        \
    ARR_MUTEX_LOCK_WRITE(a->mutex);                                              \
    if (a->size == 0) { ARR_MUTEX_UNLOCK_WRITE(a->mutex); return false; }        \
    TYPE val_to_output = a->data[a->size-1];                                     \
    a->size--;                                                                   \
    if (out) *out = val_to_output;                                               \
    ARR_MUTEX_UNLOCK_WRITE(a->mutex);                                            \
    return true;                                                                 \
}                                                                                \
bool NAME##_remove(NAME* a, size_t idx, TYPE* out) {                             \
    if (!a) return false;                                                        \
    ARR_MUTEX_LOCK_WRITE(a->mutex);                                              \
    if (idx>=a->size) { ARR_MUTEX_UNLOCK_WRITE(a->mutex); return false; }        \
    TYPE val_to_output = a->data[idx];                                           \
    memmove(&a->data[idx], &a->data[idx+1], (a->size-idx-1)*sizeof(TYPE));       \
    a->size--;                                                                   \
    if (out) *out = val_to_output;                                               \
    ARR_MUTEX_UNLOCK_WRITE(a->mutex);                                            \
    return true;                                                                 \
}                                                                                \
bool NAME##_set(NAME* a, size_t idx, TYPE v) {                                   \
    if (!a) return false;                                                        \
    ARR_MUTEX_LOCK_WRITE(a->mutex);                                              \
    if (idx>=a->size) { ARR_MUTEX_UNLOCK_WRITE(a->mutex); return false; }        \
    a->data[idx]=v;                                                              \
    ARR_MUTEX_UNLOCK_WRITE(a->mutex);                                            \
    return true;                                                                 \
}                                                                                \
bool NAME##_get(const NAME* a, size_t idx, TYPE* out) {                          \
    if (!a||!out) return false;                                                  \
    ARR_MUTEX_LOCK_READ(a->mutex);                                               \
    if (idx>=a->size) { ARR_MUTEX_UNLOCK_READ(a->mutex); return false;}          \
    *out = a->data[idx];                                                         \
    ARR_MUTEX_UNLOCK_READ(a->mutex);                                             \
    return true;                                                                 \
}                                                                                \
TYPE* NAME##_get_ptr(const NAME* a, size_t idx) {                                \
    if (!a) return NULL;                                                         \
    ARR_MUTEX_LOCK_READ(a->mutex);                                               \
    if (idx>=a->size) { ARR_MUTEX_UNLOCK_READ(a->mutex); return NULL; }          \
    TYPE* p = &a->data[idx];                                                     \
    ARR_MUTEX_UNLOCK_READ(a->mutex);                                             \
    return p;                                                                    \
}                                                                                \
TYPE NAME##_head(NAME* a) {                                                      \
    TYPE v; memset(&v, 0, sizeof(TYPE));                                         \
    if (!a) return v;                                                            \
    ARR_MUTEX_LOCK_READ(a->mutex);                                               \
    if (a->size > 0) v = a->data[0];                                             \
    ARR_MUTEX_UNLOCK_READ(a->mutex);                                             \
    return v;                                                                    \
}                                                                                \
TYPE NAME##_peek(NAME* a) {                                                      \
    TYPE v; memset(&v, 0, sizeof(TYPE));                                         \
    if (!a) return v;                                                            \
    ARR_MUTEX_LOCK_READ(a->mutex);                                               \
    if (a->size > 0) v = a->data[a->size-1];                                     \
    ARR_MUTEX_UNLOCK_READ(a->mutex);                                             \
    return v;                                                                    \
}                                                                                \
bool NAME##_is_empty(NAME* a) {                                                  \
    if (!a) return true;                                                         \
    ARR_MUTEX_LOCK_READ(a->mutex);                                               \
    bool empty = (a->size == 0);                                                 \
    ARR_MUTEX_UNLOCK_READ(a->mutex);                                             \
    return empty;                                                                \
}                                                                                \
TYPE NAME##_at(const NAME* a, size_t idx) {                                      \
    TYPE v; memset(&v, 0, sizeof(TYPE));                                         \
    if (!a) return v;                                                            \
    ARR_MUTEX_LOCK_READ(a->mutex);                                               \
    if (idx < a->size) v = a->data[idx];                                         \
    ARR_MUTEX_UNLOCK_READ(a->mutex);                                             \
    return v;                                                                    \
}                                                                                \
size_t NAME##_size(const NAME* a) {                                              \
    if (!a) return 0;                                                            \
    ARR_MUTEX_LOCK_READ(a->mutex);                                               \
    size_t n = a->size;                                                          \
    ARR_MUTEX_UNLOCK_READ(a->mutex);                                             \
    return n;                                                                    \
}                                                                                \
void NAME##_clear(NAME* a) {                                                     \
    if (!a) return;                                                              \
    ARR_MUTEX_LOCK_WRITE(a->mutex);                                              \
    a->size=0;                                                                   \
    ARR_MUTEX_UNLOCK_WRITE(a->mutex);                                            \
}

#endif // ARRAY_IMPLEMENTATION
