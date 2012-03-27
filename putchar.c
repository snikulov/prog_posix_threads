/*
 * putchar.c
 *
 * Demonstrate use of stdio file locking to generate an "atomic"
 * sequence of character writes (using putchar). If run with an
 * argument of "1", or no argument, the program uses a sequence
 * of putchar_unlocked calls within flockfile/funlockfile to
 * ensure that one threads writes cannot be interleaved with
 * another's.
 *
 * With an argument of "0", the program uses putchar, without
 * file locks, to show that the writes may be interleaved.
 *
 * The putchar[_unlocked] loop is punctuated with sleep(1) calls
 * to ensure that the desired behavior is demonstrated. Without
 * some delay, even on a multiprocessor the program may often
 * fail to display the interleaved output in this simplified
 * case.
 *
 * With file locking, you can expect to see the following output:
 *
 *      thread 1
 *      thread 2
 *      thread 3
 *
 * While without file locking, you can expect to see something
 * much less predictable, but probably resembling this:
 *
 *      ttthhhiiisss   iiisss   ttthhhrrreeeaaaddd   123
 *
 */
#include <pthread.h>
#include "errors.h"

/*
 * This function writes a string (the function's arg) to stdout,
 * by locking the file stream and using putchar_unlocked to
 * write each character individually.
 */
void *lock_routine (void *arg)
{
    char *pointer;

    flockfile (stdout);
    for (pointer = arg; *pointer != '\0'; pointer++) {
        putchar_unlocked (*pointer);
        sleep (1);
    }
    funlockfile (stdout);
    return NULL;
}

/*
 * This function writes a string (the function's arg) to stdout,
 * by using putchar to write each character individually.
 * Although the internal locking of putchar prevents file stream
 * corruption, the writes of various threads may be interleaved.
 */
void *unlock_routine (void *arg)
{
    char *pointer;

    for (pointer = arg; *pointer != '\0'; pointer++) {
        putchar (*pointer);
        sleep (1);
    }
    return NULL;
}

int main (int argc, char *argv[])
{
    pthread_t thread1, thread2, thread3;
    int flock_flag = 1;
    void *(*thread_func)(void *);
    int status;

    if (argc > 1)
        flock_flag = atoi (argv[1]);
    if (flock_flag)
        thread_func = lock_routine;
    else
        thread_func = unlock_routine;
    status = pthread_create (
        &thread1, NULL, thread_func, "this is thread 1\n");
    if (status != 0)
        err_abort (status, "Create thread");
    status = pthread_create (
        &thread2, NULL, thread_func, "this is thread 2\n");
    if (status != 0)
        err_abort (status, "Create thread");
    status = pthread_create (
        &thread3, NULL, thread_func, "this is thread 3\n");
    if (status != 0)
        err_abort (status, "Create thread");
    pthread_exit (NULL);
}
