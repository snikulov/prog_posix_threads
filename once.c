/*
 * once.c
 *
 * Demonstrate the use of pthread_once() one-time
 * initialization.
 */
#include <pthread.h>
#include "errors.h"

pthread_once_t once_block = PTHREAD_ONCE_INIT;
pthread_mutex_t mutex;

/*
 * This is the one-time initialization routine. It will be
 * called exactly once, no matter how many calls to pthread_once
 * with the same control structure are made during the course of
 * the program.
 */
void once_init_routine (void)
{
    int status;

    status = pthread_mutex_init (&mutex, NULL);
    if (status != 0)
        err_abort (status, "Init Mutex");
}

/*
 * Thread start routine that calls pthread_once.
 */
void *thread_routine (void *arg)
{
    int status;

    status = pthread_once (&once_block, once_init_routine);
    if (status != 0)
        err_abort (status, "Once init");
    status = pthread_mutex_lock (&mutex);
    if (status != 0)
        err_abort (status, "Lock mutex");
    printf ("thread_routine has locked the mutex.\n");
    status = pthread_mutex_unlock (&mutex);
    if (status != 0)
        err_abort (status, "Unlock mutex");
    return NULL;
}

int main (int argc, char *argv[])
{
    pthread_t thread_id;
    char *input, buffer[64];
    int status;

    status = pthread_create (&thread_id, NULL, thread_routine, NULL);
    if (status != 0)
        err_abort (status, "Create thread");
    status = pthread_once (&once_block, once_init_routine);
    if (status != 0)
        err_abort (status, "Once init");
    status = pthread_mutex_lock (&mutex);
    if (status != 0)
        err_abort (status, "Lock mutex");
    printf ("Main has locked the mutex.\n");
    status = pthread_mutex_unlock (&mutex);
    if (status != 0)
        err_abort (status, "Unlock mutex");
    status = pthread_join (thread_id, NULL);
    if (status != 0)
        err_abort (status, "Join thread");
    return 0;
}
