/*
 * sched_thread.c
 *
 * Demonstrate dynamic scheduling policy use.
 *
 * Special note: This demonstration will fail on Solaris 2.5
 * because it does not implement SCHED_RR.
 */
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include "errors.h"

#define THREADS 5

/*
 * Structure describing each thread.
 */
typedef struct thread_tag {
    int         index;
    pthread_t   id;
} thread_t;

thread_t        threads[THREADS];
int             rr_min_priority;

/*
 * Thread start routine that will set its own priority
 */
void *thread_routine (void *arg)
{
    thread_t *self = (thread_t*)arg;
    int my_policy;
    struct sched_param my_param;
    int status;

    my_param.sched_priority = rr_min_priority + self->index;
    DPRINTF ((
        "Thread %d will set SCHED_FIFO, priority %d\n",
        self->index, my_param.sched_priority));
    status = pthread_setschedparam (
        self->id, SCHED_RR, &my_param);
    if (status != 0)
        err_abort (status, "Set sched");
    status = pthread_getschedparam (
        self->id, &my_policy, &my_param);
    if (status != 0)
        err_abort (status, "Get sched");
    printf ("thread_routine %d running at %s/%d\n",
        self->index,
        (my_policy == SCHED_FIFO ? "FIFO"
            : (my_policy == SCHED_RR ? "RR"
            : (my_policy == SCHED_OTHER ? "OTHER"
            : "unknown"))),
        my_param.sched_priority);
    return NULL;
}

int main (int argc, char *argv[])
{
    int count, status;

    rr_min_priority = sched_get_priority_min (SCHED_RR);
    if (rr_min_priority == -1) {
#ifdef sun
        if (errno == ENOSYS) {
            fprintf (stderr, "SCHED_RR is not supported.\n");
            exit (0);
        }
#endif
        errno_abort ("Get SCHED_RR min priority");
    }
    for (count = 0; count < THREADS; count++) {
        threads[count].index = count;
        status = pthread_create (
            &threads[count].id, NULL,
            thread_routine, (void*)&threads[count]);
        if (status != 0)
            err_abort (status, "Create thread");
    }
    for (count = 0; count < THREADS; count++) {
        status = pthread_join (threads[count].id, NULL);
        if (status != 0)
            err_abort (status, "Join thread");
    }
    printf ("Main exiting\n");
    return 0;
}
