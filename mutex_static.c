/*
 * mutex_static.c
 *
 * Demonstrate static initialization of a mutex.
 */
#include <pthread.h>
#include "errors.h"

/*
 * Declare a structure, with a mutex, statically initialized. This is the
 * same as using pthread_mutex_init, with the default attributes.
 */
typedef struct my_struct_tag {
    pthread_mutex_t     mutex;  /* Protects access to value */
    int                 value;  /* Access protected by mutex */
} my_struct_t;

my_struct_t data = {PTHREAD_MUTEX_INITIALIZER, 0};

int main (int argc, char *argv[])
{
    return 0;
}
