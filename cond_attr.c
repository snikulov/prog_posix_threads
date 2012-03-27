/*
 * cond_attr.c
 *
 * main() creates a condition variable using a non-default attributes object,
 * cond_attr. If the implementation supports the pshared attribute, the
 * condition variable is created "process private". (Note that, to create a
 * "process shared" condition variable, the pthread_cond_t itself must be
 * placed in shared memory that is accessible to all threads using the
 * condition variable.)
 */
#include <pthread.h>
#include "errors.h"

pthread_cond_t cond;

int main (int argc, char *argv[])
{
    pthread_condattr_t cond_attr;
    int status;

    status = pthread_condattr_init (&cond_attr);
    if (status != 0)
        err_abort (status, "Create attr");
#ifdef _POSIX_THREAD_PROCESS_SHARED
    status = pthread_condattr_setpshared (
        &cond_attr, PTHREAD_PROCESS_PRIVATE);
    if (status != 0)
        err_abort (status, "Set pshared");
#endif
    status = pthread_cond_init (&cond, &cond_attr);
    if (status != 0)
        err_abort (status, "Init cond");
    return 0;
}
