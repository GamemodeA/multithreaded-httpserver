#include "queue.h"
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

/* Structs */

// Bounded buffer queue with 'size' amount of 'void*' elements
typedef struct queue {
    int size; // Maximum number of elements in the queue
    int num_elements; // Current number of elements in the queue
    int in; // Index of next spot to insert an element
    int out; // Index of next element to remove
    void **buffer; // Bounded buffer with 'void*' elements
    pthread_mutex_t lock; // Thread lock for critical sections
    pthread_cond_t not_full; // Threads waiting for the queue to become not full
    pthread_cond_t
        not_empty; // Threads waiting for the queue to become not empty
} queue_t;

/* Constructors & Destructors */

// Returns a reference to a new queue of size 'size'
queue_t *queue_new(int size) {
    queue_t *Q = malloc(sizeof(queue_t));
    assert(Q != NULL);

    Q->size = size;
    Q->num_elements = 0;
    Q->in = 0;
    Q->out = 0;
    Q->buffer = malloc(sizeof(void *) * size);
    assert(Q->buffer != NULL);
    pthread_mutex_init(&Q->lock, NULL);
    pthread_cond_init(&Q->not_full, NULL);
    pthread_cond_init(&Q->not_empty, NULL);

    return Q;
}

// Frees all memory associated with *Q and sets *Q to NULL
void queue_delete(queue_t **q) {
    if (q == NULL || *q == NULL)
        return;
    free((*q)->buffer);
    free(*q);
    *q = NULL;
}

/* Functions */

// Push an element 'elem' onto a queue q
bool queue_push(queue_t *q, void *elem) {
    if (q == NULL)
        return false; // Fail if *q doesn't point to a queue
    pthread_mutex_lock(&q->lock); // Set lock for critical section
    while (q->num_elements == q->size) { // Don't push while queue is full
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    q->buffer[q->in] = elem; // Insert element at next available spot
    q->in = (q->in + 1) % q->size; // Advance in
    q->num_elements++; // Update num_elements

    pthread_cond_signal(&q->not_empty); // Signal that queue is no longer empty
    pthread_mutex_unlock(&q->lock); // Unlock lock and exit critical section

    return true;
}

// Pop the element at the back of the queue q and pass it to 'elem'
bool queue_pop(queue_t *q, void **elem) {
    if (q == NULL)
        return false; // Fail if *q doesn't point to a queue
    pthread_mutex_lock(&q->lock); // Set lock for critical section
    while (q->num_elements == 0) { // Don't pop while queue is empty
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    *elem = q->buffer[q->out]; // Overwrite first element to be inserted
    q->out = (q->out + 1) % q->size; // Advance out
    q->num_elements--; // Update num_elements

    pthread_cond_signal(&q->not_full); // Signal that queue is no longer full
    pthread_mutex_unlock(&q->lock); // Unlock lock and exit critical section

    return true;
}
