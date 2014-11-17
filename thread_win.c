#include "quakedef.h"
#include "thread.h"
#include <process.h>

int Thread_Init(void)
{
#ifdef THREADDISABLE
	Con_Printf("Threading disabled in this build\n");
#endif
	return 0;
}

void Thread_Shutdown(void)
{
}

qboolean Thread_HasThreads(void)
{
#ifdef THREADDISABLE
	return false;
#else
	return true;
#endif
}

void *Thread_CreateMutex(void)
{
	void *mutex = (void *)CreateMutex(NULL, FALSE, NULL);
	return mutex;
}

void Thread_DestroyMutex(void *mutex)
{
	CloseHandle(mutex);
}

int Thread_LockMutex(void *mutex)
{
	return (WaitForSingleObject(mutex, INFINITE) == WAIT_FAILED) ? -1 : 0;
}

int Thread_UnlockMutex(void *mutex)
{
	return (ReleaseMutex(mutex) == FALSE) ? -1 : 0;
}

typedef struct thread_semaphore_s
{
	HANDLE semaphore;
	volatile LONG value;
}
thread_semaphore_t;

static thread_semaphore_t *Thread_CreateSemaphore(unsigned int v)
{
	thread_semaphore_t *s = (thread_semaphore_t *)malloc(sizeof(thread_semaphore_t));
	s->semaphore = CreateSemaphore(NULL, v, 32768, NULL);
	s->value = v;
	return s;
}

static void Thread_DestroySemaphore(thread_semaphore_t *s)
{
	CloseHandle(s->semaphore);
	free(s);
}

static int Thread_WaitSemaphore(thread_semaphore_t *s, unsigned int msec)
{
	int r = WaitForSingleObject(s->semaphore, msec);
	if (r == WAIT_OBJECT_0)
	{
		InterlockedDecrement(&s->value);
		return 0;
	}
	if (r == WAIT_TIMEOUT)
		return 1;
	return -1;
}

static int Thread_PostSemaphore(thread_semaphore_t *s)
{
	InterlockedIncrement(&s->value);
	if (ReleaseSemaphore(s->semaphore, 1, NULL))
		return 0;
	InterlockedDecrement(&s->value);
	return -1;
}

typedef struct thread_cond_s
{
	HANDLE mutex;
	int waiting;
	int signals;
	thread_semaphore_t *sem;
	thread_semaphore_t *done;
}
thread_cond_t;

void *Thread_CreateCond(void)
{
	thread_cond_t *c = (thread_cond_t *)malloc(sizeof(thread_cond_t));
	c->mutex = CreateMutex(NULL, FALSE, NULL);
	c->sem = Thread_CreateSemaphore(0);
	c->done = Thread_CreateSemaphore(0);
	c->waiting = 0;
	c->signals = 0;
	return c;
}

void Thread_DestroyCond(void *cond)
{
	thread_cond_t *c = (thread_cond_t *)cond;
	Thread_DestroySemaphore(c->sem);
	Thread_DestroySemaphore(c->done);
	CloseHandle(c->mutex);
	free(c); // Izy's Patch
}

int Thread_CondSignal(void *cond)
{
	thread_cond_t *c = (thread_cond_t *)cond;
	int n;
	WaitForSingleObject(c->mutex, INFINITE);
	n = c->waiting - c->signals;
	if (n > 0)
	{
		c->signals++;
		Thread_PostSemaphore(c->sem);
	}
	ReleaseMutex(c->mutex);
	if (n > 0)
		Thread_WaitSemaphore(c->done, INFINITE);
	return 0;
}

int Thread_CondBroadcast(void *cond)
{
	thread_cond_t *c = (thread_cond_t *)cond;
	int i = 0;
	int n = 0;
	WaitForSingleObject(c->mutex, INFINITE);
	n = c->waiting - c->signals;
	if (n > 0)
	{
		c->signals += n;
		for (i = 0;i < n;i++)
			Thread_PostSemaphore(c->sem);
	}
	ReleaseMutex(c->mutex);
	for (i = 0;i < n;i++)
		Thread_WaitSemaphore(c->done, INFINITE);
	return 0;
}

int Thread_CondWait(void *cond, void *mutex)
{
	thread_cond_t *c = (thread_cond_t *)cond;
	int waitresult;

	WaitForSingleObject(c->mutex, INFINITE);
	c->waiting++;
	ReleaseMutex(c->mutex);

	ReleaseMutex(mutex);

	waitresult = Thread_WaitSemaphore(c->sem, INFINITE);
	WaitForSingleObject(c->mutex, INFINITE);
	if (c->signals > 0)
	{
		if (waitresult > 0)
			Thread_WaitSemaphore(c->sem, INFINITE);
		Thread_PostSemaphore(c->done);
		c->signals--;
	}
	c->waiting--;
	ReleaseMutex(c->mutex);

	WaitForSingleObject(mutex, INFINITE);
	return waitresult;
}

typedef struct threadwrapper_s
{
	HANDLE handle;
	unsigned int threadid;
	int result;
	int (*fn)(void *);
	void *data;
}
threadwrapper_t;

unsigned int __stdcall Thread_WrapperFunc(void *d)
{
	threadwrapper_t *w = (threadwrapper_t *)d;
	w->result = w->fn(w->data);
	_endthreadex(w->result);
	return w->result;
}

void *Thread_CreateThread(int (*fn)(void *), void *data)
{
	threadwrapper_t *w = (threadwrapper_t *)malloc(sizeof(threadwrapper_t));
	w->fn = fn;
	w->data = data;
	w->threadid = 0;
	w->result = 0;
	w->handle = (HANDLE)_beginthreadex(NULL, 0, Thread_WrapperFunc, (void *)w, 0, &w->threadid);
	return (void *)w;
}

void Thread_WaitThread(void *d, int *retval)
{
	threadwrapper_t *w = (threadwrapper_t *)d;
	WaitForSingleObject(w->handle, INFINITE);
	CloseHandle(w->handle);
	if(retval)
		*retval = w->result;
	free(w);
}

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


void Thread_SetThreadPriorityLow(void)
{
	/* Thread Priority added by Izy (izy from http://www.izysoftware.com/) */
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
}

void Thread_SetThreadPriorityNormal(void)
{
	/* Thread Priority added by Izy (izy from http://www.izysoftware.com/) */
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
}

void Thread_SetThreadPriorityHigh(void)
{
	/* Thread Priority added by Izy (izy from http://www.izysoftware.com/) */
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
}
