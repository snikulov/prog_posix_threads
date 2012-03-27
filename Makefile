# Digital UNIX 4.0 compilation flags:
CFLAGS=-std1 -pthread -g -w1 $(DEBUGFLAGS)
RTFLAGS=-lrt

# Solaris 2.5 compilation flags:
#CFLAGS=-D_POSIX_C_SOURCE=199506 -D_REENTRANT -Xa -lpthread -g $(DEBUGFLAGS)
#RTFLAGS=-lposix4

SOURCES=alarm.c	alarm_cond.c	alarm_fork.c	alarm_mutex.c	\
	alarm_thread.c	atfork.c	backoff.c	\
	barrier_main.c	cancel.c	cancel_async.c	cancel_cleanup\
	cancel_disable.c cancel_subcontract.c	cond.c	cond_attr.c	\
	crew.c cond_dynamic.c	cond_static.c	flock.c	getlogin.c hello.c \
	inertia.c	lifecycle.c	mutex_attr.c	\
	mutex_dynamic.c	mutex_static.c	once.c	pipe.c	putchar.c	\
	rwlock_main.c	rwlock_try_main.c		\
	sched_attr.c	sched_thread.c	semaphore_signal.c	\
	semaphore_wait.c	server.c	sigev_thread.c	\
	sigwait.c	susp.c	thread.c \
	thread_attr.c	thread_error.c	trylock.c	tsd_destructor.c \
	tsd_once.c	workq_main.c
PROGRAMS=$(SOURCES:.c=)
all:	${PROGRAMS}
alarm_mutex:
	${CC} ${CFLAGS} ${RTFLAGS} ${LDFLAGS} -o $@ alarm_mutex.c
backoff:
	${CC} ${CFLAGS} ${RTFLAGS} ${LDFLAGS} -o $@ backoff.c
sched_attr:
	${CC} ${CFLAGS} ${RTFLAGS} ${LDFLAGS} -o $@ sched_attr.c
sched_thread:
	${CC} ${CFLAGS} ${RTFLAGS} ${LDFLAGS} -o $@ sched_thread.c
semaphore_signal:
	${CC} ${CFLAGS} ${RTFLAGS} ${LDFLAGS} -o $@ semaphore_signal.c
semaphore_wait:
	${CC} ${CFLAGS} ${RTFLAGS} ${LDFLAGS} -o $@ semaphore_wait.c
sigev_thread:
	${CC} ${CFLAGS} ${RTFLAGS} ${LDFLAGS} -o $@ sigev_thread.c
susp:
	${CC} ${CFLAGS} ${RTFLAGS} ${LDFLAGS} -o $@ susp.c
rwlock_main: rwlock.c rwlock.h rwlock_main.c
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ rwlock_main.c rwlock.c
rwlock_try_main: rwlock.h rwlock.c rwlock_try_main.c
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ rwlock_try_main.c rwlock.c
barrier_main: barrier.h barrier.c barrier_main.c
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ barrier_main.c barrier.c
workq_main: workq.h workq.c workq_main.c
	${CC} ${CFLAGS} ${RTFLAGS} ${LDFLAGS} -o $@ workq_main.c workq.c
clean:
	@rm -rf $(PROGRAMS) *.o
recompile:	clean all
