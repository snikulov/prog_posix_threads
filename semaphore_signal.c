/*
 * semaphore_signal.c
 *
 * Demonstrate use of POSIX semaphores to wake a thread from
 * within a signal catching function.
 *
 * Special notes: This program depends upon the _POSIX_TIMERS
 * and _POSIX_SEMAPHORES features of POSIX 1003.1b-1993. It will
 * not run (and may not compile) without them.
 */
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include "errors.h"

sem_t   semaphore;

/*
 * Signal catching function.
 */
void signal_catcher (int sig)
{
    if (sem_post (&semaphore) == -1)
        errno_abort ("Post semaphore");
}

/*
 * Thread start routine which waits on the semaphore.
 */
void *sem_waiter (void *arg)
{
    int number = (int)arg;
    int counter;

    /*
     * Each thread waits 5 times.
     */
    for (counter = 1; counter <= 5; counter++) {
        while (sem_wait (&semaphore) == -1) {
            if (errno != EINTR)
                errno_abort ("Wait on semaphore");
        }
        printf ("%d waking (%d)...\n", number, counter);
    }
    return NULL;
}

int main (int argc, char *argv[])
{
    int thread_count, status;
    struct sigevent sig_event;
    struct sigaction sig_action;
    sigset_t sig_mask;
    timer_t timer_id;
    struct itimerspec timer_val;
    pthread_t sem_waiters[5];

#if !defined(_POSIX_SEMAPHORES) || !defined(_POSIX_TIMERS)
# if !defined(_POSIX_SEMAPHORES)
    printf ("This system does not support POSIX semaphores\n");
# endif
# if !defined(_POSIX_TIMERS)
    printf ("This system does not support POSIX timers\n");
# endif
    return -1;
#else
    sem_init (&semaphore, 0, 0);

    /*
     * Create 5 threads to wait on a semaphore.
     */
    for (thread_count = 0; thread_count < 5; thread_count++) {
        status = pthread_create (
                &sem_waiters[thread_count], NULL,
                sem_waiter, (void*)thread_count);
        if (status != 0)
            err_abort (status, "Create thread");
    }

    /*
     * Set up a repeating timer using signal number SIGRTMIN,
     * set to occur every 2 seconds.
     */
    sig_event.sigev_value.sival_int = 0;
    sig_event.sigev_signo = SIGRTMIN;
    sig_event.sigev_notify = SIGEV_SIGNAL;
    if (timer_create (CLOCK_REALTIME, &sig_event, &timer_id) == -1)
        errno_abort ("Create timer");
    sigemptyset (&sig_mask);
    sigaddset (&sig_mask, SIGRTMIN);
    sig_action.sa_handler = signal_catcher;
    sig_action.sa_mask = sig_mask;
    sig_action.sa_flags = 0;
    if (sigaction (SIGRTMIN, &sig_action, NULL) == -1)
        errno_abort ("Set signal action");
    timer_val.it_interval.tv_sec = 2;
    timer_val.it_interval.tv_nsec = 0;
    timer_val.it_value.tv_sec = 2;
    timer_val.it_value.tv_nsec = 0;
    if (timer_settime (timer_id, 0, &timer_val, NULL) == -1)
        errno_abort ("Set timer");

    /*
     * Wait for all threads to complete.
     */
    for (thread_count = 0; thread_count < 5; thread_count++) {
        status = pthread_join (sem_waiters[thread_count], NULL);
        if (status != 0)
            err_abort (status, "Join thread");
    }
    return 0;
#endif
}
