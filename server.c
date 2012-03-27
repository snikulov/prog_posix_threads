/*
 * server.c
 *
 * Demonstrate a client/server threading model.
 *
 * Special notes: On a Solaris system, call thr_setconcurrency()
 * to allow interleaved thread execution, since threads are not
 * timesliced.
 */
#include <pthread.h>
#include <math.h>
#include "errors.h"

#define CLIENT_THREADS  4               /* Number of clients */

#define REQ_READ        1               /* Read with prompt */
#define REQ_WRITE       2               /* Write */
#define REQ_QUIT        3               /* Quit server */

/*
 * Internal to server "package" -- one for each request.
 */
typedef struct request_tag {
    struct request_tag  *next;          /* Link to next */
    int                 operation;      /* Function code */
    int                 synchronous;    /* Non-zero if synchronous */
    int                 done_flag;      /* Predicate for wait */
    pthread_cond_t      done;           /* Wait for completion */
    char                prompt[32];     /* Prompt string for reads */
    char                text[128];      /* Read/write text */
} request_t;

/*
 * Static context for the server
 */
typedef struct tty_server_tag {
    request_t           *first;
    request_t           *last;
    int                 running;
    pthread_mutex_t     mutex;
    pthread_cond_t      request;
} tty_server_t;

tty_server_t tty_server = {
    NULL, NULL, 0,
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};

/*
 * Main program data
 */

int client_threads;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t clients_done = PTHREAD_COND_INITIALIZER;

/*
 * The server start routine. It waits for a request to appear
 * in tty_server.requests using the request condition variable.
 * It processes requests in FIFO order. If a request is marked
 * "synchronous" (synchronous != 0), the server will set done_flag
 * and signal the request's condition variable. The client is
 * responsible for freeing the request. If the request was not
 * synchronous, the server will free the request on completion.
 */
void *tty_server_routine (void *arg)
{
    static pthread_mutex_t prompt_mutex = PTHREAD_MUTEX_INITIALIZER;
    request_t *request;
    int operation, len;
    int status;

    while (1) {
        status = pthread_mutex_lock (&tty_server.mutex);
        if (status != 0)
            err_abort (status, "Lock server mutex");

        /*
         * Wait for data
         */
        while (tty_server.first == NULL) {
            status = pthread_cond_wait (
                &tty_server.request, &tty_server.mutex);
            if (status != 0)
                err_abort (status, "Wait for request");
        }
        request = tty_server.first;
        tty_server.first = request->next;
        if (tty_server.first == NULL)
            tty_server.last = NULL;
        status = pthread_mutex_unlock (&tty_server.mutex);
        if (status != 0)
            err_abort (status, "Unlock server mutex");

        /*
         * Process the data
         */
        operation = request->operation;
        switch (operation) {
            case REQ_QUIT:
                break;
            case REQ_READ:
                if (strlen (request->prompt) > 0)
                    printf (request->prompt);
                if (fgets (request->text, 128, stdin) == NULL)
                    request->text[0] = '\0';
                /*
                 * Because fgets returns the newline, and we don't want it,
                 * we look for it, and turn it into a null (truncating the
                 * input) if found. It should be the last character, if it is
                 * there.
                 */
                len = strlen (request->text);
                if (len > 0 && request->text[len-1] == '\n')
                    request->text[len-1] = '\0';
                break;
            case REQ_WRITE:
                puts (request->text);
                break;
            default:
                break;
        }
        if (request->synchronous) {
            status = pthread_mutex_lock (&tty_server.mutex);
            if (status != 0)
                err_abort (status, "Lock server mutex");
            request->done_flag = 1;
            status = pthread_cond_signal (&request->done);
            if (status != 0)
                err_abort (status, "Signal server condition");
            status = pthread_mutex_unlock (&tty_server.mutex);
            if (status != 0)
                err_abort (status, "Unlock server mutex");
        } else
            free (request);
        if (operation == REQ_QUIT)
            break;
    }
    return NULL;
}

/*
 * Request an operation
 */
void tty_server_request (
    int         operation,
    int         sync,
    const char  *prompt,
    char        *string)
{
    request_t *request;
    int status;

    status = pthread_mutex_lock (&tty_server.mutex);
    if (status != 0)
        err_abort (status, "Lock server mutex");
    if (!tty_server.running) {
        pthread_t thread;
        pthread_attr_t detached_attr;

        status = pthread_attr_init (&detached_attr);
        if (status != 0)
            err_abort (status, "Init attributes object");
        status = pthread_attr_setdetachstate (
            &detached_attr, PTHREAD_CREATE_DETACHED);
        if (status != 0)
            err_abort (status, "Set detach state");
        tty_server.running = 1;
        status = pthread_create (&thread, &detached_attr,
            tty_server_routine, NULL);
        if (status != 0)
            err_abort (status, "Create server");

        /*
         * Ignore an error in destroying the attributes object.
         * It's unlikely to fail, there's nothing useful we can
         * do about it, and it's not worth aborting the program
         * over it.
         */
        pthread_attr_destroy (&detached_attr);
    }

    /*
     * Create and initialize a request structure.
     */
    request = (request_t*)malloc (sizeof (request_t));
    if (request == NULL)
        errno_abort ("Allocate request");
    request->next = NULL;
    request->operation = operation;
    request->synchronous = sync;
    if (sync) {
        request->done_flag = 0;
        status = pthread_cond_init (&request->done, NULL);
        if (status != 0)
            err_abort (status, "Init request condition");
    }
    if (prompt != NULL)
        strncpy (request->prompt, prompt, 32);
    else
        request->prompt[0] = '\0';
    if (operation == REQ_WRITE && string != NULL)
        strncpy (request->text, string, 128);
    else
        request->text[0] = '\0';

    /*
     * Add the request to the queue, maintaining the first and
     * last pointers.
     */
    if (tty_server.first == NULL) {
        tty_server.first = request;
        tty_server.last = request;
    } else {
        (tty_server.last)->next = request;
        tty_server.last = request;
    }

    /*
     * Tell the server that a request is available.
     */
    status = pthread_cond_signal (&tty_server.request);
    if (status != 0)
        err_abort (status, "Wake server");

    /*
     * If the request was "synchronous", then wait for a reply.
     */
    if (sync) {
        while (!request->done_flag) {
            status = pthread_cond_wait (
                &request->done, &tty_server.mutex);
            if (status != 0)
                err_abort (status, "Wait for sync request");
        }
        if (operation == REQ_READ) {
            if (strlen (request->text) > 0)
                strcpy (string, request->text);
            else
                string[0] = '\0';
        }
        status = pthread_cond_destroy (&request->done);
        if (status != 0)
            err_abort (status, "Destroy request condition");
        free (request);
    }
    status = pthread_mutex_unlock (&tty_server.mutex);
    if (status != 0)
        err_abort (status, "Unlock mutex");
}

/*
 * Client routine -- multiple copies will request server.
 */
void *client_routine (void *arg)
{
    int my_number = (int)arg, loops;
    char prompt[32];
    char string[128], formatted[128];
    int status;

    sprintf (prompt, "Client %d> ", my_number);
    while (1) {
        tty_server_request (REQ_READ, 1, prompt, string);
        if (strlen (string) == 0)
            break;
        for (loops = 0; loops < 4; loops++) {
            sprintf (
                formatted, "(%d#%d) %s", my_number, loops, string);
            tty_server_request (REQ_WRITE, 0, NULL, formatted);
            sleep (1);
        }
    }
    status = pthread_mutex_lock (&client_mutex);
    if (status != 0)
        err_abort (status, "Lock client mutex");
    client_threads--;
    if (client_threads <= 0) {
        status = pthread_cond_signal (&clients_done);
        if (status != 0)
            err_abort (status, "Signal clients done");
    }
    status = pthread_mutex_unlock (&client_mutex);
    if (status != 0)
        err_abort (status, "Unlock client mutex");
    return NULL;
}

int main (int argc, char *argv[])
{
    pthread_t thread;
    int count;
    int status;

#ifdef sun
    /*
     * On Solaris 2.5, threads are not timesliced. To ensure
     * that our threads can run concurrently, we need to
     * increase the concurrency level to CLIENT_THREADS.
     */
    DPRINTF (("Setting concurrency level to %d\n", CLIENT_THREADS));
    thr_setconcurrency (CLIENT_THREADS);
#endif

    /*
     * Create CLIENT_THREADS clients.
     */
    client_threads = CLIENT_THREADS;
    for (count = 0; count < CLIENT_THREADS; count++) {
        status = pthread_create (&thread, NULL,
            client_routine, (void*)count);
        if (status != 0)
            err_abort (status, "Create client thread");
    }
    status = pthread_mutex_lock (&client_mutex);
    if (status != 0)
        err_abort (status, "Lock client mutex");
    while (client_threads > 0) {
        status = pthread_cond_wait (&clients_done, &client_mutex);
        if (status != 0)
            err_abort (status, "Wait for clients to finish");
    }
    status = pthread_mutex_unlock (&client_mutex);
    if (status != 0)
        err_abort (status, "Unlock client mutex");
    printf ("All clients done\n");
    tty_server_request (REQ_QUIT, 1, NULL, NULL);
    return 0;
}
