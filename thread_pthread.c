#include "quakedef.h"
#include "thread.h"
#ifdef THREADRECURSIVE
#define __USE_UNIX98
#include <pthread.h>
#endif
#include <stdint.h>


int Thread_Init(void)
{
	return 0;
}

void Thread_Shutdown(void)
{
}

qboolean Thread_HasThreads(void)
{
	return true;
}

void *Thread_CreateMutex(void)
{
#ifdef THREADRECURSIVE
	pthread_mutexattr_t    attr;
#endif
	pthread_mutex_t *mutexp = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
#ifdef THREADRECURSIVE
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(mutexp, &attr);
	pthread_mutexattr_destroy(&attr);
#else
	pthread_mutex_init(mutexp, NULL);
#endif
	return mutexp;
}

void Thread_DestroyMutex(void *mutex)
{
	pthread_mutex_t *mutexp = (pthread_mutex_t *) mutex;
	pthread_mutex_destroy(mutexp);
	free(mutexp);
}

int Thread_LockMutex(void *mutex)
{
	pthread_mutex_t *mutexp = (pthread_mutex_t *) mutex;
	return pthread_mutex_lock(mutexp);
}

int Thread_UnlockMutex(void *mutex)
{
	pthread_mutex_t *mutexp = (pthread_mutex_t *) mutex;
	return pthread_mutex_unlock(mutexp);
}

void *Thread_CreateCond(void)
{
	pthread_cond_t *condp = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
	pthread_cond_init(condp, NULL);
	return condp;
}

void Thread_DestroyCond(void *cond)
{
	pthread_cond_t *condp = (pthread_cond_t *) cond;
	pthread_cond_destroy(condp);
	free(condp);
}

int Thread_CondSignal(void *cond)
{
	pthread_cond_t *condp = (pthread_cond_t *) cond;
	return pthread_cond_signal(condp);
}

int Thread_CondBroadcast(void *cond)
{
	pthread_cond_t *condp = (pthread_cond_t *) cond;
	return pthread_cond_broadcast(condp);
}

int Thread_CondWait(void *cond, void *mutex)
{
	pthread_cond_t *condp = (pthread_cond_t *) cond;
	pthread_mutex_t *mutexp = (pthread_mutex_t *) mutex;
	return pthread_cond_wait(condp, mutexp);
}

void *Thread_CreateThread(int (*fn)(void *), void *data)
{
	pthread_t *threadp = (pthread_t *) malloc(sizeof(pthread_t));
	int r = pthread_create(threadp, NULL, (void * (*) (void *)) fn, data);
	if(r)
	{
		free(threadp);
		return NULL;
	}
	return threadp;
}

void Thread_WaitThread(void *thread, int *retval)
{
	union
	{
		void *p;
		int i;
	} status;
	pthread_t *threadp = (pthread_t *) thread;
	pthread_join(*threadp, &status.p);
	if(retval)
		*retval = status.i;
	free(threadp);
}

#ifdef PTHREAD_BARRIER_SERIAL_THREAD
void *Thread_CreateBarrier(unsigned int count)
{
	pthread_barrier_t *b = (pthread_barrier_t *) malloc(sizeof(pthread_barrier_t));
	pthread_barrier_init(b, NULL, count);
	return (void *) b;
}

void Thread_DestroyBarrier(void *barrier)
{
	pthread_barrier_t *b = (pthread_barrier_t *) barrier;
	pthread_barrier_destroy(b);
	free(b); // Izy's Patch
}

void Thread_WaitBarrier(void *barrier)
{
	pthread_barrier_t *b = (pthread_barrier_t *) barrier;
	pthread_barrier_wait(b);
}
#else
// standard barrier implementation using conds and mutexes
// see: http://www.howforge.com/implementing-barrier-in-pthreads
typedef struct {
	unsigned int needed;
	unsigned int called;
	void *mutex;
	void *cond;
} barrier_t;

void *Thread_CreateBarrier(unsigned int count)
{
	volatile barrier_t *b = (volatile barrier_t *) malloc(sizeof(barrier_t));
	b->needed = count;
	b->called = 0;
	b->mutex = Thread_CreateMutex();
	b->cond = Thread_CreateCond();
	return (void *) b;
}

void Thread_DestroyBarrier(void *barrier)
{
	volatile barrier_t *b = (volatile barrier_t *) barrier;
	Thread_DestroyMutex(b->mutex);
	Thread_DestroyCond(b->cond);
	free((void*)b); // Izy's Patch
}

void Thread_WaitBarrier(void *barrier)
{
	volatile barrier_t *b = (volatile barrier_t *) barrier;
	Thread_LockMutex(b->mutex);
	b->called++;
	if (b->called == b->needed) {
		b->called = 0;
		Thread_CondBroadcast(b->cond);
	} else {
		do {
			Thread_CondWait(b->cond, b->mutex);
		} while(b->called);
	}
	Thread_UnlockMutex(b->mutex);
}
#endif


void Thread_SetThreadPriorityLow(void)
{
	/* Thread Priority added by Izy (izy from http://www.izysoftware.com/) */
	pthread_attr_t attr;
	int policy = 0;
	int min_prio_for_policy;
	pthread_attr_init(&attr);
	pthread_attr_getschedpolicy(&attr, &policy);
	min_prio_for_policy = sched_get_priority_min(policy);
	pthread_setschedprio(pthread_self(), min_prio_for_policy);
	pthread_attr_destroy(&attr);
}

void Thread_SetThreadPriorityNormal(void)
{
	/* Thread Priority added by Izy (izy from http://www.izysoftware.com/) */
	pthread_attr_t attr;
	int policy = 0;
	int min_prio_for_policy;
	int max_prio_for_policy;
	int normal_prio_for_policy;
	pthread_attr_init(&attr);
	pthread_attr_getschedpolicy(&attr, &policy);
	min_prio_for_policy = sched_get_priority_min(policy);
	max_prio_for_policy = sched_get_priority_max(policy);
	if(min_prio_for_policy >= 0 && max_prio_for_policy >= 0)
		normal_prio_for_policy = (max_prio_for_policy-min_prio_for_policy)/2+min_prio_for_policy;
	else
		if(min_prio_for_policy < 0 && max_prio_for_policy <= 0)
			normal_prio_for_policy = (min_prio_for_policy-max_prio_for_policy)/2+max_prio_for_policy;
		else
			normal_prio_for_policy = (max_prio_for_policy+min_prio_for_policy)/2;
	pthread_setschedprio(pthread_self(), normal_prio_for_policy);
	pthread_attr_destroy(&attr);
}

void Thread_SetThreadPriorityHigh(void)
{
	/* Thread Priority added by Izy (izy from http://www.izysoftware.com/) */
	pthread_attr_t attr;
	int policy = 0;
	int max_prio_for_policy;
	pthread_attr_init(&attr);
	pthread_attr_getschedpolicy(&attr, &policy);
	max_prio_for_policy = sched_get_priority_max(policy);
	pthread_setschedprio(pthread_self(), max_prio_for_policy);
	pthread_attr_destroy(&attr);
}
