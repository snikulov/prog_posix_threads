/*
 * semaphore_wait.c
 *
 * Demonstrate use of semaphores for synchronization.
 */
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include "errors.h"

sem_t semaphore;

/*
 * Thread start routine to wait on a semaphore.
 */
void *sem_waiter (void *arg)
{
    long num = (long)arg;

    printf ("Thread %ld waiting\n", num);
    if (sem_wait (&semaphore) == -1)
        errno_abort ("Wait on semaphore");
    printf ("Thread %ld resuming\n", num);
    return NULL;
}

int main (int argc, char *argv[])
{
    int thread_count;
    pthread_t sem_waiters[5];
    int status;

#if !defined(_POSIX_SEMAPHORES)
    printf ("This system does not support POSIX semaphores\n");
    return -1;
#else

    if (sem_init (&semaphore, 0, 0) == -1)
        errno_abort ("Init semaphore");

    /*
     * Create 5 threads to wait concurrently on the semaphore.
     */
    for (thread_count = 0; thread_count < 5; thread_count++) {
        status = pthread_create (
            &sem_waiters[thread_count], NULL,
            sem_waiter, (void*)thread_count);
        if (status != 0)
            err_abort (status, "Create thread");
    }

    sleep (2);

    /*
     * "Broadcast" the semaphore by repeatedly posting until the
     * count of waiters goes to 0.
     */
    while (1) {
        int sem_value;

        if (sem_getvalue (&semaphore, &sem_value) == -1)
            errno_abort ("Get semaphore value");
        if (sem_value >= 0)
            break;
        printf ("Posting from main: %d\n", sem_value);
        if (sem_post (&semaphore) == -1)
            errno_abort ("Post semaphore");
    }

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
