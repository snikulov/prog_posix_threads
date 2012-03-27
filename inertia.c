/*
 * inertia.c
 *
 * Demonstrate a class of programming error, assuming that
 * threads can't start right away.
 *
 * Special notes: On a Solaris 2.5 uniprocessor, this test can't
 * fail unless a second LWP is created by calling
 * thr_setconcurrency() because threads are not timesliced.
 */
#include <pthread.h>
#include "errors.h"

void *printer_thread (void *arg)
{
    char *string = *(char**)arg;

    printf ("%s\n", string);
    return NULL;
}

int main (int argc, char *argv[])
{
    pthread_t printer_id;
    char *string_ptr;
    int i, status;

#ifdef sun
    /*
     * On Solaris 2.5, threads are not timesliced. To ensure
     * that our two threads can run concurrently, we need to
     * increase the concurrency level to 2.
     */
    DPRINTF (("Setting concurrency level to 2\n"));
    thr_setconcurrency (2);
#endif
    string_ptr = "Before value";
    status = pthread_create (
        &printer_id, NULL, printer_thread, (void*)&string_ptr);
    if (status != 0)
        err_abort (status, "Create thread");

    /*
     * Give the thread a chance to get started if it's going to run
     * in parallel, but not enough that the current thread is likely
     * to be timesliced. (This is a tricky balance, and the loop may
     * need to be adjusted on your system before you can see the bug.)
     */
    for (i = 0; i < 10000000; i++);

    string_ptr = "After value";
    status = pthread_join (printer_id, NULL);
    if (status != 0)
        err_abort (status, "Join thread");
    return 0;
}
