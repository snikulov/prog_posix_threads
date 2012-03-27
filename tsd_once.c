/*
 * tsd_once.c
 *
 * Demonstrate use of pthread_once to initialize something
 * exactly once within a multithreaded program.
 *
 * Note that it is often easier to use a statically initialized
 * mutex to accomplish the same result.
 */
#include <pthread.h>
#include "errors.h"

/*
 * Structure used as the value for thread-specific data key.
 */
typedef struct tsd_tag {
    pthread_t   thread_id;
    char        *string;
} tsd_t;

pthread_key_t tsd_key;           /* Thread-specific data key */
pthread_once_t key_once = PTHREAD_ONCE_INIT;

/*
 * One-time initialization routine used with the pthread_once
 * control block.
 */
void once_routine (void)
{
    int status;

    printf ("initializing key\n");
    status = pthread_key_create (&tsd_key, NULL);
    if (status != 0)
        err_abort (status, "Create key");
}

/*
 * Thread start routine that uses pthread_once to dynamically
 * create a thread-specific data key.
 */
void *thread_routine (void *arg)
{
    tsd_t *value;
    int status;

    status = pthread_once (&key_once, once_routine);
    if (status != 0)
        err_abort (status, "Once init");
    value = (tsd_t*)malloc (sizeof (tsd_t));
    if (value == NULL)
        errno_abort ("Allocate key value");
    status = pthread_setspecific (tsd_key, value);
    if (status != 0)
        err_abort (status, "Set tsd");
    printf ("%s set tsd value %p\n", arg, value);
    value->thread_id = pthread_self ();
    value->string = (char*)arg;
    value = (tsd_t*)pthread_getspecific (tsd_key);
    printf ("%s starting...\n", value->string);
    sleep (2);
    value = (tsd_t*)pthread_getspecific (tsd_key);
    printf ("%s done...\n", value->string);
    return NULL;    
}

void main (int argc, char *argv[])
{
    pthread_t thread1, thread2;
    int status;

    status = pthread_create (
        &thread1, NULL, thread_routine, "thread 1");
    if (status != 0)
        err_abort (status, "Create thread 1");
    status = pthread_create (
        &thread2, NULL, thread_routine, "thread 2");
    if (status != 0)
        err_abort (status, "Create thread 2");
    pthread_exit (NULL);
}
