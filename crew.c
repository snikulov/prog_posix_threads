/*
 * crew.c
 *
 * Demonstrate a work crew implementing a simple parallel search
 * through a directory tree.
 *
 * Special notes: On a Solaris 2.5 uniprocessor, this test will
 * not produce interleaved output unless extra LWPs are created
 * by calling thr_setconcurrency(), because threads are not
 * timesliced.
 */
#include <sys/types.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>
#include "errors.h"

#define CREW_SIZE       4

/*
 * Queued items of work for the crew. One is queued by
 * crew_start, and each worker may queue additional items.
 */
typedef struct work_tag {
    struct work_tag     *next;          /* Next work item */
    char                *path; /* Directory or file */
    char                *string;        /* Search string */
} work_t, *work_p;

/*
 * One of these is initialized for each worker thread in the
 * crew. It contains the "identity" of each worker.
 */
typedef struct worker_tag {
    int                 index;          /* Thread's index */
    pthread_t           thread;         /* Thread for stage */
    struct crew_tag     *crew;          /* Pointer to crew */
} worker_t, *worker_p;

/*
 * The external "handle" for a work crew. Contains the
 * crew synchronization state and staging area.
 */
typedef struct crew_tag {
    int                 crew_size;      /* Size of array */
    worker_t            crew[CREW_SIZE];/* Crew members */
    long                work_count;     /* Count of work items */
    work_t              *first, *last;  /* First & last work item */
    pthread_mutex_t     mutex;          /* Mutex for crew data */
    pthread_cond_t      done;           /* Wait for crew done */
    pthread_cond_t      go;             /* Wait for work */
} crew_t, *crew_p;

size_t  path_max;                       /* Filepath length */
size_t  name_max;                       /* Name length */

/*
 * The thread start routine for crew threads. Waits until "go"
 * command, processes work items until requested to shut down.
 */
void *worker_routine (void *arg)
{
    worker_p mine = (worker_t*)arg;
    crew_p crew = mine->crew;
    work_p work, new_work;
    struct stat filestat;
    struct dirent *entry;
    int status;

    /*
     * "struct dirent" is funny, because POSIX doesn't require
     * the definition to be more than a header for a variable
     * buffer. Thus, allocate a "big chunk" of memory, and use
     * it as a buffer.
     */
    entry = (struct dirent*)malloc (
        sizeof (struct dirent) + name_max);
    if (entry == NULL)
        errno_abort ("Allocating dirent");
    
    status = pthread_mutex_lock (&crew->mutex);
    if (status != 0)
        err_abort (status, "Lock crew mutex");

    /*
     * There won't be any work when the crew is created, so wait
     * until something's put on the queue.
     */
    while (crew->work_count == 0) {
        status = pthread_cond_wait (&crew->go, &crew->mutex);
        if (status != 0)
            err_abort (status, "Wait for go");
    }

    status = pthread_mutex_unlock (&crew->mutex);
    if (status != 0)
        err_abort (status, "Unlock mutex");

    DPRINTF (("Crew %d starting\n", mine->index));

    /*
     * Now, as long as there's work, keep doing it.
     */
    while (1) {
        /*
         * Wait while there is nothing to do, and
         * the hope of something coming along later. If
         * crew->first is NULL, there's no work. But if
         * crew->work_count goes to zero, we're done.
         */
        status = pthread_mutex_lock (&crew->mutex);
        if (status != 0)
            err_abort (status, "Lock crew mutex");

        DPRINTF (("Crew %d top: first is %#lx, count is %d\n",
                  mine->index, crew->first, crew->work_count));
        while (crew->first == NULL) {
            status = pthread_cond_wait (&crew->go, &crew->mutex);
            if (status != 0)
                err_abort (status, "Wait for work");
        }

        DPRINTF (("Crew %d woke: %#lx, %d\n",
                  mine->index, crew->first, crew->work_count));

        /*
         * Remove and process a work item
         */
        work = crew->first;
        crew->first = work->next;
        if (crew->first == NULL)
            crew->last = NULL;

        DPRINTF (("Crew %d took %#lx, leaves first %#lx, last %#lx\n",
                  mine->index, work, crew->first, crew->last));

        status = pthread_mutex_unlock (&crew->mutex);
        if (status != 0)
            err_abort (status, "Unlock mutex");

        /*
         * We have a work item. Process it, which may involve
         * queuing new work items.
         */
        status = lstat (work->path, &filestat);

        if (S_ISLNK (filestat.st_mode))
            printf (
                "Thread %d: %s is a link, skipping.\n",
                mine->index,
                work->path);
        else if (S_ISDIR (filestat.st_mode)) {
            DIR *directory;
            struct dirent *result;

            /*
             * If the file is a directory, search it and place
             * all files onto the queue as new work items.
             */
            directory = opendir (work->path);
            if (directory == NULL) {
                fprintf (
                    stderr, "Unable to open directory %s: %d (%s)\n",
                    work->path,
                    errno, strerror (errno));
                continue;
            }
            
            while (1) {
                status = readdir_r (directory, entry, &result);
                if (status != 0) {
                    fprintf (
                        stderr,
                        "Unable to read directory %s: %d (%s)\n",
                        work->path,
                        status, strerror (status));
                    break;
                }
                if (result == NULL)
                    break;              /* End of directory */
                
                /*
                 * Ignore "." and ".." entries.
                 */
                if (strcmp (entry->d_name, ".") == 0)
                    continue;
                if (strcmp (entry->d_name, "..") == 0)
                    continue;
                new_work = (work_p)malloc (sizeof (work_t));
                if (new_work == NULL)
                    errno_abort ("Unable to allocate space");
                new_work->path = (char*)malloc (path_max);
                if (new_work->path == NULL)
                    errno_abort ("Unable to allocate path");
                strcpy (new_work->path, work->path);
                strcat (new_work->path, "/");
                strcat (new_work->path, entry->d_name);
                new_work->string = work->string;
                new_work->next = NULL;
                status = pthread_mutex_lock (&crew->mutex);
                if (status != 0)
                    err_abort (status, "Lock mutex");
                if (crew->first == NULL) {
                    crew->first = new_work;
                    crew->last = new_work;
                } else {
                    crew->last->next = new_work;
                    crew->last = new_work;
                }
                crew->work_count++;
                DPRINTF ((
                    "Crew %d: add work %#lx, first %#lx, last %#lx, %d\n",
                    mine->index, new_work, crew->first,
                    crew->last, crew->work_count));
                status = pthread_cond_signal (&crew->go);
                status = pthread_mutex_unlock (&crew->mutex);
                if (status != 0)
                    err_abort (status, "Unlock mutex");
            }
            
            closedir (directory);
        } else if (S_ISREG (filestat.st_mode)) {
            FILE *search;
            char buffer[256], *bufptr, *search_ptr;

            /*
             * If this is a file, not a directory, then search
             * it for the string.
             */
            search = fopen (work->path, "r");
            if (search == NULL)
                fprintf (
                    stderr, "Unable to open %s: %d (%s)\n",
                    work->path,
                    errno, strerror (errno));
            else {

                while (1) {
                    bufptr = fgets (
                        buffer, sizeof (buffer), search);
                    if (bufptr == NULL) {
                        if (feof (search))
                            break;
                        if (ferror (search)) {
                            fprintf (
                                stderr,
                                "Unable to read %s: %d (%s)\n",
                                work->path,
                                errno, strerror (errno));
                            break;
                        }
                    }
                    search_ptr = strstr (buffer, work->string);
                    if (search_ptr != NULL) {
                        flockfile (stdout);
                        printf (
                            "Thread %d found \"%s\" in %s\n",
                            mine->index, work->string, work->path);
#if 0
                        printf ("%s\n", buffer);
#endif
                        funlockfile (stdout);
                        break;
                    }
                }
                fclose (search);
            }
        } else
            fprintf (
                stderr,
                "Thread %d: %s is type %o (%s))\n",
                mine->index,
                work->path,
                filestat.st_mode & S_IFMT,
                (S_ISFIFO (filestat.st_mode) ? "FIFO"
                 : (S_ISCHR (filestat.st_mode) ? "CHR"
                    : (S_ISBLK (filestat.st_mode) ? "BLK"
                       : (S_ISSOCK (filestat.st_mode) ? "SOCK"
                          : "unknown")))));

        free (work->path);              /* Free path buffer */
        free (work);                    /* We're done with this */

        /*
         * Decrement count of outstanding work items, and wake
         * waiters (trying to collect results or start a new
         * calculation) if the crew is now idle.
         *
         * It's important that the count be decremented AFTER
         * processing the current work item. That ensures the
         * count won't go to 0 until we're really done.
         */
        status = pthread_mutex_lock (&crew->mutex);
        if (status != 0)
            err_abort (status, "Lock crew mutex");

        crew->work_count--;
        DPRINTF (("Crew %d decremented work to %d\n", mine->index,
                  crew->work_count));
        if (crew->work_count <= 0) {
            DPRINTF (("Crew thread %d done\n", mine->index));
            status = pthread_cond_broadcast (&crew->done);
            if (status != 0)
                err_abort (status, "Wake waiters");
            status = pthread_mutex_unlock (&crew->mutex);
            if (status != 0)
                err_abort (status, "Unlock mutex");
            break;
        }

        status = pthread_mutex_unlock (&crew->mutex);
        if (status != 0)
            err_abort (status, "Unlock mutex");

    }

    free (entry);
    return NULL;
}

/*
 * Create a work crew.
 */
int crew_create (crew_t *crew, int crew_size)
{
    int crew_index;
    int status;

    /*
     * We won't create more than CREW_SIZE members
     */
    if (crew_size > CREW_SIZE)
        return EINVAL;

    crew->crew_size = crew_size;
    crew->work_count = 0;
    crew->first = NULL;
    crew->last = NULL;

    /*
     * Initialize synchronization objects
     */
    status = pthread_mutex_init (&crew->mutex, NULL);
    if (status != 0)
        return status;
    status = pthread_cond_init (&crew->done, NULL);
    if (status != 0)
        return status;
    status = pthread_cond_init (&crew->go, NULL);
    if (status != 0)
        return status;

    /*
     * Create the worker threads.
     */
    for (crew_index = 0; crew_index < CREW_SIZE; crew_index++) {
        crew->crew[crew_index].index = crew_index;
        crew->crew[crew_index].crew = crew;
        status = pthread_create (&crew->crew[crew_index].thread,
            NULL, worker_routine, (void*)&crew->crew[crew_index]);
        if (status != 0)
            err_abort (status, "Create worker");
    }
    return 0;
}

/*
 * Pass a file path to a work crew previously created
 * using crew_create
 */
int crew_start (
    crew_p crew,
    char *filepath,
    char *search)
{
    work_p request;
    int status;

    status = pthread_mutex_lock (&crew->mutex);
    if (status != 0)
        return status;

    /*
     * If the crew is busy, wait for them to finish.
     */
    while (crew->work_count > 0) {
        status = pthread_cond_wait (&crew->done, &crew->mutex);
        if (status != 0) {
            pthread_mutex_unlock (&crew->mutex);
            return status;
        }
    }

    errno = 0;
    path_max = pathconf (filepath, _PC_PATH_MAX);
    if (path_max == -1) {
        if (errno == 0)
            path_max = 1024;             /* "No limit" */
        else
            errno_abort ("Unable to get PATH_MAX");
    }
    errno = 0;
    name_max = pathconf (filepath, _PC_NAME_MAX);
    if (name_max == -1) {
        if (errno == 0)
            name_max = 256;             /* "No limit" */
        else
            errno_abort ("Unable to get NAME_MAX");
    }
    DPRINTF ((
        "PATH_MAX for %s is %ld, NAME_MAX is %ld\n",
        filepath, path_max, name_max));
    path_max++;                         /* Add null byte */
    name_max++;                         /* Add null byte */
    request = (work_p)malloc (sizeof (work_t));
    if (request == NULL)
        errno_abort ("Unable to allocate request");
    DPRINTF (("Requesting %s\n", filepath));
    request->path = (char*)malloc (path_max);
    if (request->path == NULL)
        errno_abort ("Unable to allocate path");
    strcpy (request->path, filepath);
    request->string = search;
    request->next = NULL;
    if (crew->first == NULL) {
        crew->first = request;
        crew->last = request;
    } else {
        crew->last->next = request;
        crew->last = request;
    }

    crew->work_count++;
    status = pthread_cond_signal (&crew->go);
    if (status != 0) {
        free (crew->first);
        crew->first = NULL;
        crew->work_count = 0;
        pthread_mutex_unlock (&crew->mutex);
        return status;
    }
    while (crew->work_count > 0) {
        status = pthread_cond_wait (&crew->done, &crew->mutex);
        if (status != 0)
            err_abort (status, "waiting for crew to finish");
    }
    status = pthread_mutex_unlock (&crew->mutex);
    if (status != 0)
        err_abort (status, "Unlock crew mutex");
    return 0;
}

/*
 * The main program to "drive" the crew...
 */
int main (int argc, char *argv[])
{
    crew_t my_crew;
    char line[128], *next;
    int status;

    if (argc < 3) {
        fprintf (stderr, "Usage: %s string path\n", argv[0]);
        return -1;
    }

#ifdef sun
    /*
     * On Solaris 2.5, threads are not timesliced. To ensure
     * that our threads can run concurrently, we need to
     * increase the concurrency level to CREW_SIZE.
     */
    DPRINTF (("Setting concurrency level to %d\n", CREW_SIZE));
    thr_setconcurrency (CREW_SIZE);
#endif
    status = crew_create (&my_crew, CREW_SIZE);
    if (status != 0)
        err_abort (status, "Create crew");

    status = crew_start (&my_crew, argv[2], argv[1]);
    if (status != 0)
        err_abort (status, "Start crew");

    return 0;
}
