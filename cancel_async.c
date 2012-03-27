/*
 * cancel_async.c
 *
 * Demonstrate asynchronous cancellation of a compute-bound
 * thread.
 *
 * Special notes: On a Solaris 2.5 uniprocessor, this test will
 * hang unless a second LWP is created by calling
 * thr_setconcurrency() because threads are not timesliced.
 */
#include <pthread.h>
#include "errors.h"

#define SIZE    10      /* array size */

static int matrixa[SIZE][SIZE];
static int matrixb[SIZE][SIZE];
static int matrixc[SIZE][SIZE];

#ifdef DEBUG
void print_array (int matrix[SIZE][SIZE])
{
    int i, j;
    int first;

    for (i = 0; i < SIZE; i++) {
        printf ("[");
        first = 1;
        for (j = 0; j < SIZE; j++) {
            if (!first)
                printf (",");
            printf ("%x", matrix[i][j]);
            first = 0;
        }
        printf ("]\n");
    }
        
}
#endif

/*
 * Loop until cancelled. The thread can be cancelled at any
 * point within the inner loop, where asynchronous cancellation
 * is enabled. The loop multiplies the two matrices matrixa
 * and matrixb.
 */
void *thread_routine (void *arg)
{
    int cancel_type, status;
    int i, j, k, value = 1;
    
    /*
     * Initialize the matrices to something arbitrary.
     */
    for (i = 0; i < SIZE; i++)
        for (j = 0; j < SIZE; j++) {
            matrixa[i][j] = i;
            matrixb[i][j] = j;
        }

    while (1) {
        /*
         * Compute the matrix product of matrixa and matrixb.
         */
        status = pthread_setcanceltype (
                PTHREAD_CANCEL_ASYNCHRONOUS,
                &cancel_type);
        if (status != 0)
            err_abort (status, "Set cancel type");
        for (i = 0; i < SIZE; i++)
            for (j = 0; j < SIZE; j++) {
                matrixc[i][j] = 0;
                for (k = 0; k < SIZE; k++)
                    matrixc[i][j] += matrixa[i][k] * matrixb[k][j];
            }
        status = pthread_setcanceltype (
                cancel_type,
                &cancel_type);
        if (status != 0)
            err_abort (status, "Set cancel type");

        /*
         * Copy the result (matrixc) into matrixa to start again
         */
        for (i = 0; i < SIZE; i++)
            for (j = 0; j < SIZE; j++)
                matrixa[i][j] = matrixc[i][j];
    }
}

int main (int argc, char *argv[])
{
    pthread_t thread_id;
    void *result;
    int status;

#ifdef sun
    /*
     * On Solaris 2.5, threads are not timesliced. To ensure
     * that our two threads can run concurrently, we need to
     * increase the concurrency level to 2.
     */
    DPRINTF (("Setting concurrency level to 2\n"));
    thr_setconcurrency (2);
#endif
    status = pthread_create (
        &thread_id, NULL, thread_routine, NULL);
    if (status != 0)
        err_abort (status, "Create thread");
    sleep (1);
    status = pthread_cancel (thread_id);
    if (status != 0)
        err_abort (status, "Cancel thread");
    status = pthread_join (thread_id, &result);
    if (status != 0)
        err_abort (status, "Join thread");
    if (result == PTHREAD_CANCELED)
        printf ("Thread cancelled\n");
    else
        printf ("Thread was not cancelled\n");
#ifdef DEBUG
    printf ("Matrix a:\n");
    print_array (matrixa);
    printf ("\nMatrix b:\n");
    print_array (matrixb);
    printf ("\nMatrix c:\n");
    print_array (matrixc);
#endif
    return 0;
}
