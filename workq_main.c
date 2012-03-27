/*
 * workq_main.c
 *
 * Demonstrate a use of work queues.
 */
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "workq.h"
#include "errors.h"

#define ITERATIONS      25

typedef struct power_tag {
    int         value;
    int         power;
} power_t;

typedef struct engine_tag {
    struct engine_tag   *link;
    pthread_t           thread_id;
    int                 calls;
} engine_t;

pthread_key_t engine_key;       /* Keep track of active engines */
pthread_mutex_t engine_list_mutex = PTHREAD_MUTEX_INITIALIZER;
engine_t *engine_list_head = NULL;
workq_t workq;

/*
 * Thread-specific data destructor routine for engine_key
 */
void destructor (void *value_ptr)
{
    engine_t *engine = (engine_t*)value_ptr;

    pthread_mutex_lock (&engine_list_mutex);
    engine->link = engine_list_head;
    engine_list_head = engine;
    pthread_mutex_unlock (&engine_list_mutex);
}

/*
 * This is the routine called by the work queue servers to
 * perform operations in parallel.
 */
void engine_routine (void *arg)
{
    engine_t *engine;
    power_t *power = (power_t*)arg;
    int result, count;
    int status;

    engine = pthread_getspecific (engine_key);
    if (engine == NULL) {
        engine = (engine_t*)malloc (sizeof (engine_t));
        status = pthread_setspecific (
            engine_key, (void*)engine);
        if (status != 0)
            err_abort (status, "Set tsd");
        engine->thread_id = pthread_self ();
        engine->calls = 1;
    } else
        engine->calls++;
    result = 1;
    printf (
        "Engine: computing %d^%d\n",
        power->value, power->power);
    for (count = 1; count <= power->power; count++)
        result *= power->value;
    free (arg);
}

/*
 * Thread start routine that issues work queue requests.
 */
void *thread_routine (void *arg)
{
    power_t *element;
    int count;
    unsigned int seed = (unsigned int)time (NULL);
    int status;

    /*
     * Loop, making requests.
     */
    for (count = 0; count < ITERATIONS; count++) {
        element = (power_t*)malloc (sizeof (power_t));
        if (element == NULL)
            errno_abort ("Allocate element");
        element->value = rand_r (&seed) % 20;
        element->power = rand_r (&seed) % 7;
        DPRINTF ((
            "Request: %d^%d\n",
            element->value, element->power));
        status = workq_add (&workq, (void*)element);
        if (status != 0)
            err_abort (status, "Add to work queue");
        sleep (rand_r (&seed) % 5);
    }
    return NULL;
}

int main (int argc, char *argv[])
{
    pthread_t thread_id;
    engine_t *engine;
    int count = 0, calls = 0;
    int status;

    status = pthread_key_create (&engine_key, destructor);
    if (status != 0)
        err_abort (status, "Create key");
    status = workq_init (&workq, 4, engine_routine);
    if (status != 0)
        err_abort (status, "Init work queue");
    status = pthread_create (&thread_id, NULL, thread_routine, NULL);
    if (status != 0)
        err_abort (status, "Create thread");
    (void)thread_routine (NULL);
    status = pthread_join (thread_id, NULL);
    if (status != 0)
        err_abort (status, "Join thread");
    status = workq_destroy (&workq);
    if (status != 0)
        err_abort (status, "Destroy work queue");

    /*
     * By now, all of the engine_t structures have been placed
     * on the list (by the engine thread destructors), so we
     * can count and summarize them.
     */
    engine = engine_list_head;
    while (engine != NULL) {
        count++;
        calls += engine->calls;
        printf ("engine %d: %d calls\n", count, engine->calls);
        engine = engine->link;
    }
    printf ("%d engine threads processed %d calls\n",
        count, calls);
    return 0;
}
