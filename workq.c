/*
 * workq.c
 *
 * This file implements the interfaces for a "work queue"
 * manager. A "manager object" is created with several
 * parameters, including the required size of a work queue
 * entry, the maximum desired degree of parallelism (number of
 * threads to service the queue), and the address of an
 * execution engine routine.
 *
 * The application requests a work queue entry from the manager,
 * fills in the application-specific fields, and returns it to
 * the queue manager for processing. The manager will create a
 * new thread to service the queue if all current threads are
 * busy and the maximum level of parallelism has not yet been
 * reached.
 *
 * The manager will dequeue items and present them to the
 * processing engine until the queue is empty; at that point,
 * processing threads will begin to shut down. (They will be
 * restarted when work appears.)
 */
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include "errors.h"
#include "workq.h"

/*
 * Thread start routine to serve the work queue.
 */
static void *workq_server (void *arg)
{
    struct timespec timeout;
    workq_t *wq = (workq_t *)arg;
    workq_ele_t *we;
    int status, timedout;

    /*
     * We don't need to validate the workq_t here... we don't
     * create server threads until requests are queued (the
     * queue has been initialized by then!) and we wait for all
     * server threads to terminate before destroying a work
     * queue.
     */
    DPRINTF (("A worker is starting\n"));
    status = pthread_mutex_lock (&wq->mutex);
    if (status != 0)
        return NULL;

    while (1) {
        timedout = 0;
        DPRINTF (("Worker waiting for work\n"));
        clock_gettime (CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 2;

        while (wq->first == NULL && !wq->quit) {
            /*
             * Server threads time out after spending 2 seconds
             * waiting for new work, and exit.
             */
            status = pthread_cond_timedwait (
                    &wq->cv, &wq->mutex, &timeout);
            if (status == ETIMEDOUT) {
                DPRINTF (("Worker wait timed out\n"));
                timedout = 1;
                break;
            } else if (status != 0) {
                /*
                 * This shouldn't happen, so the work queue
                 * package should fail. Because the work queue
                 * API is asynchronous, that would add
                 * complication. Because the chances of failure
                 * are slim, I choose to avoid that
                 * complication. The server thread will return,
                 * and allow another server thread to pick up
                 * the work later. Note that, if this was the
                 * only server thread, the queue won't be
                 * serviced until a new work item is
                 * queued. That could be fixed by creating a new
                 * server here.
                 */
                DPRINTF ((
                    "Worker wait failed, %d (%s)\n",
                    status, strerror (status)));
                wq->counter--;
                pthread_mutex_unlock (&wq->mutex);
                return NULL;
            }
        }
        DPRINTF (("Work queue: 0x%p, quit: %d\n",
		  wq->first, wq->quit));
        we = wq->first;

        if (we != NULL) {
            wq->first = we->next;
            if (wq->last == we)
                wq->last = NULL;
            status = pthread_mutex_unlock (&wq->mutex);
            if (status != 0)
                return NULL;
            DPRINTF (("Worker calling engine\n"));
            wq->engine (we->data);
            free (we);
            status = pthread_mutex_lock (&wq->mutex);
            if (status != 0)
                return NULL;
        }

        /*
         * If there are no more work requests, and the servers
         * have been asked to quit, then shut down.
         */
        if (wq->first == NULL && wq->quit) {
            DPRINTF (("Worker shutting down\n"));
            wq->counter--;

            /*
             * NOTE: Just to prove that every rule has an
             * exception, I'm using the "cv" condition for two
             * separate predicates here.  That's OK, since the
             * case used here applies only once during the life
             * of a work queue -- during rundown. The overhead
             * is minimal and it's not worth creating a separate
             * condition variable that would be waited and
             * signaled exactly once!
             */
            if (wq->counter == 0)
                pthread_cond_broadcast (&wq->cv);
            pthread_mutex_unlock (&wq->mutex);
            return NULL;
        }

        /*
         * If there's no more work, and we wait for as long as
         * we're allowed, then terminate this server thread.
         */
        if (wq->first == NULL && timedout) {
            DPRINTF (("engine terminating due to timeout.\n"));
            wq->counter--;
            break;
        }
    }

    pthread_mutex_unlock (&wq->mutex);
    DPRINTF (("Worker exiting\n"));
    return NULL;
}

/*
 * Initialize a work queue.
 */
int workq_init (workq_t *wq, int threads, void (*engine)(void *arg))
{
    int status;

    status = pthread_attr_init (&wq->attr);
    if (status != 0)
        return status;
    status = pthread_attr_setdetachstate (
        &wq->attr, PTHREAD_CREATE_DETACHED);
    if (status != 0) {
        pthread_attr_destroy (&wq->attr);
        return status;
    }
    status = pthread_mutex_init (&wq->mutex, NULL);
    if (status != 0) {
        pthread_attr_destroy (&wq->attr);
        return status;
    }
    status = pthread_cond_init (&wq->cv, NULL);
    if (status != 0) {
        pthread_mutex_destroy (&wq->mutex);
        pthread_attr_destroy (&wq->attr);
        return status;
    }
    wq->quit = 0;                       /* not time to quit */
    wq->first = wq->last = NULL;        /* no queue entries */
    wq->parallelism = threads;          /* max servers */
    wq->counter = 0;                    /* no server threads yet */
    wq->idle = 0;                       /* no idle servers */
    wq->engine = engine;
    wq->valid = WORKQ_VALID;
    return 0;
}

/*
 * Destroy a work queue.
 */
int workq_destroy (workq_t *wq)
{
    int status, status1, status2;

    if (wq->valid != WORKQ_VALID)
        return EINVAL;
    status = pthread_mutex_lock (&wq->mutex);
    if (status != 0)
        return status;
    wq->valid = 0;                 /* prevent any other operations */

    /*
     * Check whether any threads are active, and run them down:
     *
     * 1.       set the quit flag
     * 2.       broadcast to wake any servers that may be asleep
     * 4.       wait for all threads to quit (counter goes to 0)
     *          Because we don't use join, we don't need to worry
     *          about tracking thread IDs.
     */
    if (wq->counter > 0) {
        wq->quit = 1;
        /* if any threads are idling, wake them. */
        if (wq->idle > 0) {
            status = pthread_cond_broadcast (&wq->cv);
            if (status != 0) {
                pthread_mutex_unlock (&wq->mutex);
                return status;
            }
        }

        /*
         * Just to prove that every rule has an exception, I'm
         * using the "cv" condition for two separate predicates
         * here. That's OK, since the case used here applies
         * only once during the life of a work queue -- during
         * rundown. The overhead is minimal and it's not worth
         * creating a separate condition variable that would be
         * waited and signalled exactly once!
         */
        while (wq->counter > 0) {
            status = pthread_cond_wait (&wq->cv, &wq->mutex);
            if (status != 0) {
                pthread_mutex_unlock (&wq->mutex);
                return status;
            }
        }       
    }
    status = pthread_mutex_unlock (&wq->mutex);
    if (status != 0)
        return status;
    status = pthread_mutex_destroy (&wq->mutex);
    status1 = pthread_cond_destroy (&wq->cv);
    status2 = pthread_attr_destroy (&wq->attr);
    return (status ? status : (status1 ? status1 : status2));
}

/*
 * Add an item to a work queue.
 */
int workq_add (workq_t *wq, void *element)
{
    workq_ele_t *item;
    pthread_t id;
    int status;

    if (wq->valid != WORKQ_VALID)
        return EINVAL;

    /*
     * Create and initialize a request structure.
     */
    item = (workq_ele_t *)malloc (sizeof (workq_ele_t));
    if (item == NULL)
        return ENOMEM;
    item->data = element;
    item->next = NULL;
    status = pthread_mutex_lock (&wq->mutex);
    if (status != 0) {
        free (item);
        return status;
    }

    /*
     * Add the request to the end of the queue, updating the
     * first and last pointers.
     */
    if (wq->first == NULL)
        wq->first = item;
    else
        wq->last->next = item;
    wq->last = item;

    /*
     * if any threads are idling, wake one.
     */
    if (wq->idle > 0) {
        status = pthread_cond_signal (&wq->cv);
        if (status != 0) {
            pthread_mutex_unlock (&wq->mutex);
            return status;
        }
    } else if (wq->counter < wq->parallelism) {
        /*
         * If there were no idling threads, and we're allowed to
         * create a new thread, do so.
         */
        DPRINTF (("Creating new worker\n"));
        status = pthread_create (
            &id, &wq->attr, workq_server, (void*)wq);
        if (status != 0) {
            pthread_mutex_unlock (&wq->mutex);
            return status;
        }
        wq->counter++;
    }
    pthread_mutex_unlock (&wq->mutex);
    return 0;
}
