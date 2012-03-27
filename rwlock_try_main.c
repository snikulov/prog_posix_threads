/*
 * rwlock_try_main.c
 *
 * Demonstrate use of non-blocking read-write locks.
 *
 * Special notes: On a Solaris system, call thr_setconcurrency()
 * to allow interleaved thread execution, since threads are not
 * timesliced.
 */
#include <pthread.h>
#include "rwlock.h"
#include "errors.h"

#define THREADS         5
#define ITERATIONS      1000
#define DATASIZE        15

/*
 * Keep statistics for each thread.
 */
typedef struct thread_tag {
    int         thread_num;
    pthread_t   thread_id;
    int         r_collisions;
    int         w_collisions;
    int         updates;
    int         interval;
} thread_t;

/*
 * Read-write lock and shared data
 */
typedef struct data_tag {
    rwlock_t    lock;
    int         data;
    int         updates;
} data_t;

thread_t threads[THREADS];
data_t data[DATASIZE];

/*
 * Thread start routine that uses read-write locks
 */
void *thread_routine (void *arg)
{
    thread_t *self = (thread_t*)arg;
    int iteration;
    int element;
    int status;

    element = 0;                        /* Current data element */

    for (iteration = 0; iteration < ITERATIONS; iteration++) {
        if ((iteration % self->interval) == 0) {
            status = rwl_writetrylock (&data[element].lock);
            if (status == EBUSY)
                self->w_collisions++;
            else if (status == 0) {
                data[element].data++;
                data[element].updates++;
                self->updates++;
                rwl_writeunlock (&data[element].lock);
            } else
                err_abort (status, "Try write lock");
        } else {
            status = rwl_readtrylock (&data[element].lock);
            if (status == EBUSY)
                self->r_collisions++;
            else if (status != 0) {
                err_abort (status, "Try read lock");
            } else {
                if (data[element].data != data[element].updates)
                    printf ("%d: data[%d] %d != %d\n",
                        self->thread_num, element,
                        data[element].data, data[element].updates);
                rwl_readunlock (&data[element].lock);
            }
        }

        element++;
        if (element >= DATASIZE)
            element = 0;
    }
    return NULL;
}

int main (int argc, char *argv[])
{
    int count, data_count;
    unsigned int seed = 1;
    int thread_updates = 0, data_updates = 0;
    int status;

#ifdef sun
    /*
     * On Solaris 2.5, threads are not timesliced. To ensure
     * that our threads can run concurrently, we need to
     * increase the concurrency level to THREADS.
     */
    DPRINTF (("Setting concurrency level to %d\n", THREADS));
    thr_setconcurrency (THREADS);
#endif

    /*
     * Initialize the shared data.
     */
    for (data_count = 0; data_count < DATASIZE; data_count++) {
        data[data_count].data = 0;
        data[data_count].updates = 0;
        rwl_init (&data[data_count].lock);
    }

    /*
     * Create THREADS threads to access shared data.
     */
    for (count = 0; count < THREADS; count++) {
        threads[count].thread_num = count;
        threads[count].r_collisions = 0;
        threads[count].w_collisions = 0;
        threads[count].updates = 0;
        threads[count].interval = rand_r (&seed) % ITERATIONS;
        status = pthread_create (&threads[count].thread_id,
            NULL, thread_routine, (void*)&threads[count]);
        if (status != 0)
            err_abort (status, "Create thread");
    }

    /*
     * Wait for all threads to complete, and collect
     * statistics.
     */
    for (count = 0; count < THREADS; count++) {
        status = pthread_join (threads[count].thread_id, NULL);
        if (status != 0)
            err_abort (status, "Join thread");
        thread_updates += threads[count].updates;
        printf ("%02d: interval %d, updates %d, "
                "r_collisions %d, w_collisions %d\n",
            count, threads[count].interval,
            threads[count].updates,
            threads[count].r_collisions, threads[count].w_collisions);
    }

    /*
     * Collect statistics for the data.
     */
    for (data_count = 0; data_count < DATASIZE; data_count++) {
        data_updates += data[data_count].updates;
        printf ("data %02d: value %d, %d updates\n",
            data_count, data[data_count].data, data[data_count].updates);
        rwl_destroy (&data[data_count].lock);
    }

    return 0;
}
