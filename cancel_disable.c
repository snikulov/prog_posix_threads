/*
 * cancel_disable.c
 *
 * Demonstrate running a section of code with cancellation
 * disabled.
 */
#include <pthread.h>
#include "errors.h"

static int counter;

/*
 * Thread start routine.
 */
void *thread_routine (void *arg)
{
    int state;
    int status;

    for (counter = 0; ; counter++) {

        /*
         * Each 755 iterations, disable cancellation and sleep
         * for one second.
         *
         * Each 1000 iterations, test for a pending cancel by
         * calling pthread_testcancel().
         */
        if ((counter % 755) == 0) {
            status = pthread_setcancelstate (
                PTHREAD_CANCEL_DISABLE, &state);
            if (status != 0)
                err_abort (status, "Disable cancel");
            sleep (1);
            status = pthread_setcancelstate (
                state, &state);
            if (status != 0)
                err_abort (status, "Restore cancel");
        } else
            if ((counter % 1000) == 0)
                pthread_testcancel ();
    }
}

int main (int argc, char *argv[])
{
    pthread_t thread_id;
    void *result;
    int status;

    status = pthread_create (
        &thread_id, NULL, thread_routine, NULL);
    if (status != 0)
        err_abort (status, "Create thread");
    sleep (2);
    status = pthread_cancel (thread_id);
    if (status != 0)
        err_abort (status, "Cancel thread");

    status = pthread_join (thread_id, &result);
    if (status != 0)
        err_abort (status, "Join thread");
    if (result == PTHREAD_CANCELED)
        printf ("Thread cancelled at iteration %d\n", counter);
    else
        printf ("Thread was not cancelled\n");
    return 0;
}
