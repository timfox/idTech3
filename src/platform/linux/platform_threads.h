#pragma once

#include <pthread.h>

#if !defined(_POSIX_SPIN_LOCKS) || (_POSIX_SPIN_LOCKS <= 0)
#ifndef PLATFORM_DISABLE_SPINLOCK_FALLBACK
typedef struct {
	pthread_mutex_t mutex;
} pthread_spinlock_t;

static inline int pthread_spin_init(pthread_spinlock_t *lock, int pshared) {
	(void)pshared;
	return pthread_mutex_init(&lock->mutex, NULL);
}

static inline int pthread_spin_destroy(pthread_spinlock_t *lock) {
	return pthread_mutex_destroy(&lock->mutex);
}

static inline int pthread_spin_lock(pthread_spinlock_t *lock) {
	return pthread_mutex_lock(&lock->mutex);
}

static inline int pthread_spin_unlock(pthread_spinlock_t *lock) {
	return pthread_mutex_unlock(&lock->mutex);
}
#define PLATFORM_HAS_SPINLOCK_FALLBACK 1
#else
#define PLATFORM_HAS_SPINLOCK_FALLBACK 0
#endif
#else
#define PLATFORM_HAS_SPINLOCK_FALLBACK 0
#endif
