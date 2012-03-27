/*
 * barrier_main.c
 *
 * Demonstrate use of barriers, using the barrier implementation
 * in barrier.c.
 */
#include <pthread.h>
#include "barrier.h"
#include "errors.h"

#define THREADS 5
#define ARRAY 6
#define INLOOPS 1000
#define OUTLOOPS 10

/*
 * Keep track of each thread
 */
typedef struct thread_tag {
    pthread_t   thread_id;
    int         number;
    int         increment;
    int         array[ARRAY];
} thread_t;

barrier_t barrier;
thread_t thread[THREADS];

/*
 * Start routine for threads.
 */
void *thread_routine (void *arg)
{
    thread_t *self = (thread_t*)arg;    /* Thread's thread_t */
    int in_loop, out_loop, count, status;
    
    /*
     * Loop through OUTLOOPS barrier cycles.
     */
    for (out_loop = 0; out_loop < OUTLOOPS; out_loop++) {
        status = barrier_wait (&barrier);
        if (status > 0)
            err_abort (status, "Wait on barrier");

        /*
         * This inner loop just adds a value to each element in
         * the working array.
         */
        for (in_loop = 0; in_loop < INLOOPS; in_loop++)
            for (count = 0; count < ARRAY; count++)
                self->array[count] += self->increment;

        status = barrier_wait (&barrier);
        if (status > 0)
            err_abort (status, "Wait on barrier");

        /*
         * The barrier causes one thread to return with the
         * special return status -1. The thread receiving this
         * value increments each element in the shared array.
         */
        if (status == -1) {
            int thread_num;

            for (thread_num = 0; thread_num < THREADS; thread_num++)
                thread[thread_num].increment += 1;
        }
    }
    return NULL;
}

int main (int arg, char *argv[])
{
    int thread_count, array_count;
    int status;

    barrier_init (&barrier, THREADS);

    /*
     * Create a set of threads that will use the barrier.
     */
    for (thread_count = 0; thread_count < THREADS; thread_count++) {
        thread[thread_count].increment = thread_count;
        thread[thread_count].number = thread_count;

        for (array_count = 0; array_count < ARRAY; array_count++)
            thread[thread_count].array[array_count] = array_count + 1;

        status = pthread_create (&thread[thread_count].thread_id,
            NULL, thread_routine, (void*)&thread[thread_count]);
        if (status != 0)
            err_abort (status, "Create thread");
    }

    /*
     * Now join with each of the threads.
     */
    for (thread_count = 0; thread_count < THREADS; thread_count++) {
        status = pthread_join (thread[thread_count].thread_id, NULL);
        if (status != 0)
            err_abort (status, "Join thread");

        printf ("%02d: (%d) ", thread_count, thread[thread_count].increment);

        for (array_count = 0; array_count < ARRAY; array_count++)
            printf ("%010u ", thread[thread_count].array[array_count]);
        printf ("\n");
    }

    /*
     * To be thorough, destroy the barrier.
     */
    barrier_destroy (&barrier);
    return 0;
}
