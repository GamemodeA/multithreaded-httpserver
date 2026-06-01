#include "rwlock.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

/* Structs */

// A read/write lock that allows multiple readers to hold the lock, but only a single writer
typedef struct rwlock {
    PRIORITY priority; // The lock's priority in a reader/writer lock
    int nway; // The lock's priority in an n-way lock

    int readers_active; // The number of readers reading a critical section
    int writers_active; // The number of writers actively writing (never > 1)
    int readers_waiting; // The number of readers waiting to read
    int writers_waiting; // The number of writers waiting to write
    int readers_since_write; // The number of readers that ran since the last writer wrote

    pthread_mutex_t mutex;
    pthread_cond_t readers_cv;
    pthread_cond_t writers_cv;
} rwlock_t;

/* Constructors and Destructors */

// Returns a reference to a new lock of priority p
rwlock_t *rwlock_new(PRIORITY p, uint32_t n) {
    // Allocate required memory
    rwlock_t *L = malloc(sizeof(rwlock_t));
    assert(L != NULL);

    // Initialize all structure members
    L->priority = p;
    L->nway = n;
    L->readers_active = 0;
    L->writers_active = 0;
    L->readers_waiting = 0;
    L->writers_waiting = 0;
    L->readers_since_write = 0;

    pthread_mutex_init(&L->mutex, NULL);
    pthread_cond_init(&L->readers_cv, NULL);
    pthread_cond_init(&L->writers_cv, NULL);

    return L;
}

// Frees all memory associated with Lock *rw and sets *rw to NULL
void rwlock_delete(rwlock_t **rw) {
    if (rw == NULL || *rw == NULL)
        return;
    free(*rw);
    *rw = NULL;
}

/* Helper Functions */

// Determines if the reader should continue waiting before executing
static bool reader_should_wait(rwlock_t *rw) {
    // Writer currently using lock
    if (rw->writers_active > 0) {
        return true;
    }
    // WRITERS: block new readers if writers is waiting
    if (rw->priority == WRITERS && rw->writers_waiting > 0) {
        return true;
    }
    // N_WAY: once quota reached and writers waiting, stop admitting readers
    if (rw->priority == N_WAY && rw->writers_waiting > 0
        && rw->readers_since_write >= rw->nway) {
        return true;
    }
    return false;
}

// Determines if the writer should continue waiting before executing
static bool writer_should_wait(rwlock_t *rw) {
    // Wait if another writer is active
    if (rw->writers_active > 0) {
        return true;
    }
    // Wait if a reader is active
    if (rw->readers_active > 0) {
        return true;
    }
    return false;
}

/* Member Functions */

// Acquire rw for reading
void reader_lock(rwlock_t *rw) {
    pthread_mutex_lock(&rw->mutex);
    rw->readers_waiting++;

    // Wait until a writer holds the lock
    while (reader_should_wait(rw)) {
        pthread_cond_wait(&rw->readers_cv, &rw->mutex);
    }

    // Update n-way quota
    if (rw->priority == N_WAY && rw->writers_waiting > 0) {
        rw->readers_since_write++;
    }

    // Run reader
    rw->readers_waiting--;
    rw->readers_active++;
    pthread_mutex_unlock(&rw->mutex);
}

// Release rw for reading
void reader_unlock(rwlock_t *rw) {
    pthread_mutex_lock(&rw->mutex);
    rw->readers_active--;

    // Handle and update other readers
    if (rw->readers_active == 0) {
        switch (rw->priority) {
        case READERS:
            // Prefer readers if there are any waiting
            if (rw->readers_waiting > 0) {
                pthread_cond_broadcast(&rw->readers_cv);
            }
            // Otherwise allow one writer
            else if (rw->writers_waiting > 0) {
                pthread_cond_signal(&rw->writers_cv);
            }
            break;
        case WRITERS:
            // Prefer writers
            if (rw->writers_waiting > 0) {
                pthread_cond_signal(&rw->writers_cv);
            }
            // Otherwise allow readers
            else if (rw->readers_waiting > 0) {
                pthread_cond_broadcast(&rw->readers_cv);
            }
            break;
        case N_WAY:
            // If quota 0 and writers waiting, run writer, else run readers
            if (rw->writers_waiting > 0
                && rw->readers_since_write >= rw->nway) {
                pthread_cond_signal(&rw->writers_cv);
            } else if (rw->readers_waiting > 0) {
                pthread_cond_broadcast(&rw->readers_cv);
            } else if (rw->writers_waiting > 0) {
                pthread_cond_signal(&rw->writers_cv);
            }
            break;
        }
    }
    pthread_mutex_unlock(&rw->mutex);
}

// Acquire rw for writing
void writer_lock(rwlock_t *rw) {
    pthread_mutex_lock(&rw->mutex);
    rw->writers_waiting++;

    // Wait until no readers/writers are active
    while (writer_should_wait(rw)) {
        pthread_cond_wait(&rw->writers_cv, &rw->mutex);
    }
    // Reset n-way quota
    if (rw->priority == N_WAY) {
        rw->readers_since_write = 0;
    }

    // Run writer
    rw->writers_waiting--;
    rw->writers_active = 1;
    pthread_mutex_unlock(&rw->mutex);
}

// Release rw for writing
void writer_unlock(rwlock_t *rw) {
    pthread_mutex_lock(&rw->mutex);
    rw->writers_active = 0;

    // Handle and update other threads
    switch (rw->priority) {
    case READERS:
        // Readers get preference
        if (rw->readers_waiting > 0) {
            pthread_cond_broadcast(&rw->readers_cv);
        }
        // Otherwise let 1 writer proceed
        else if (rw->writers_waiting > 0) {
            pthread_cond_signal(&rw->writers_cv);
        }
        break;
    case WRITERS:
        // Writers get preference
        if (rw->writers_waiting > 0) {
            pthread_cond_signal(&rw->writers_cv);
        }
        // Otherwise allow readers
        else if (rw->readers_waiting > 0) {
            pthread_cond_broadcast(&rw->readers_cv);
        }
        break;
    case N_WAY:
        // After a writer finishes, allow readers first
        if (rw->readers_waiting > 0) {
            pthread_cond_broadcast(&rw->readers_cv);
        }
        // If no readers are waiting, let writer continue
        else if (rw->writers_waiting > 0) {
            pthread_cond_signal(&rw->writers_cv);
        }
        break;
    }
    pthread_mutex_unlock(&rw->mutex);
}
