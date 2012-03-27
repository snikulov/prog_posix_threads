/*
 * hello.c
 *
 * Create a very simple thread that says "Hello world", just
 * because it has to be here.
 */
#include <pthread.h>
#include "errors.h"

void *hello_world (void *arg)
{
    printf ("Hello world\n");
    return NULL;
}

int main (int argc, char *argv[])
{
    pthread_t hello_id;
    int status;

    status = pthread_create (&hello_id, NULL, hello_world, NULL);
    if (status != 0)
        err_abort (status, "Create thread");
    status = pthread_join (hello_id, NULL);
    if (status != 0)
        err_abort (status, "Join thread");
    return 0;
}
