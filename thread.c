/*
 * thread.c
 *
 * Demonstrate a simple thread that writes to stdout while the
 * initial thread is blocked in a read from stdin.
 */
#include <pthread.h>
#include <stdio.h>

/*
 * Thread start routine that writes a message.
 */
void *writer_thread (void *arg)
{
    sleep (5);
    printf ("Thread I/O\n");
    return NULL;
}

int main (int argc, char *argv[])
{
    pthread_t   writer_id;
    char        *input, buffer[64];

    pthread_create (&writer_id, NULL, writer_thread, NULL);
    input = fgets (buffer, 64, stdin);
    if (input != NULL)
        printf ("You said %s", buffer);
    pthread_exit (NULL);
    return 0;
}
