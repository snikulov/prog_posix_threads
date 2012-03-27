/*
 * flock.c
 *
 * Demonstrate use of stdio file locking to generate an "atomic"
 * prompting sequence. The write to stdout and the read from
 * stdin cannot be separated.
 */
#include <pthread.h>
#include "errors.h"

/*
 * This routine writes a prompt to stdout (passed as the thread's
 * "arg"), and reads a response. All other I/O to stdin and stdout
 * is prevented by the file locks until both prompt and fgets are
 * complete.
 */
void *prompt_routine (void *arg)
{
    char *prompt = (char*)arg;
    char *string;
    int len;

    string = (char*)malloc (128);
    if (string == NULL)
        errno_abort ("Alloc string");
    flockfile (stdin);
    flockfile (stdout);
    printf (prompt);
    if (fgets (string, 128, stdin) == NULL)
        string[0] = '\0';
    else {
        len = strlen (string);
        if (len > 0 && string[len-1] == '\n')
            string[len-1] = '\0';
    }
    funlockfile (stdout);
    funlockfile (stdin);
    return (void*)string;
}

int main (int argc, char *argv[])
{
    pthread_t thread1, thread2, thread3;
    void *string;
    int status;

#ifdef sun
    /*
     * On Solaris 2.5, threads are not timesliced. To ensure
     * that our threads can run concurrently, we need to
     * increase the concurrency level.
     */
    DPRINTF (("Setting concurrency level to 4\n"));
    thr_setconcurrency (4);
#endif
    status = pthread_create (
        &thread1, NULL, prompt_routine, "Thread 1> ");
    if (status != 0)
        err_abort (status, "Create thread");
    status = pthread_create (
        &thread2, NULL, prompt_routine, "Thread 2> ");
    if (status != 0)
        err_abort (status, "Create thread");
    status = pthread_create (
        &thread3, NULL, prompt_routine, "Thread 3> ");
    if (status != 0)
        err_abort (status, "Create thread");
    status = pthread_join (thread1, &string);
    if (status != 0)
        err_abort (status, "Join thread");
    printf ("Thread 1: \"%s\"\n", (char*)string);
    free (string);
    status = pthread_join (thread2, &string);
    if (status != 0)
        err_abort (status, "Join thread");
    printf ("Thread 2: \"%s\"\n", (char*)string);
    free (string);
    status = pthread_join (thread3, &string);
    if (status != 0)
        err_abort (status, "Join thread");
    printf ("Thread 3: \"%s\"\n", (char*)string);
    free (string);
    return 0;
}
