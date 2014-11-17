#include <SDL.h>
#include <SDL_thread.h>
#include "quakedef.h"
#include "thread.h"

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
	void *mutex = SDL_CreateMutex();
	return mutex;
}

void Thread_DestroyMutex(void *mutex)
{
	SDL_DestroyMutex((SDL_mutex *)mutex);
}

int Thread_LockMutex(void *mutex)
{
	return SDL_LockMutex((SDL_mutex *)mutex);
}

int Thread_UnlockMutex(void *mutex)
{
	return SDL_UnlockMutex((SDL_mutex *)mutex);
}

void *Thread_CreateCond(void)
{
	void *cond = (void *)SDL_CreateCond();
	return cond;
}

void Thread_DestroyCond(void *cond)
{
	SDL_DestroyCond((SDL_cond *)cond);
}

int Thread_CondSignal(void *cond)
{
	return SDL_CondSignal((SDL_cond *)cond);
}

int Thread_CondBroadcast(void *cond)
{
	return SDL_CondBroadcast((SDL_cond *)cond);
}

int Thread_CondWait(void *cond, void *mutex)
{
	return SDL_CondWait((SDL_cond *)cond, (SDL_mutex *)mutex);
}

void *Thread_CreateThread(int (*fn)(void *), void *data)
{
#if SDL_MAJOR_VERSION == 1
	void *thread = (void *)SDL_CreateThread(fn, data);
#else
	void *thread = (void *)SDL_CreateThread(fn, "", data);
#endif
	return thread;
}

void Thread_WaitThread(void *thread, int *retval)
{
	SDL_WaitThread((SDL_Thread *)thread, retval);
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
#if SDL_VERSION_ATLEAST(2,0,3)
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
#endif
}

void Thread_SetThreadPriorityNormal(void)
{
	/* Thread Priority added by Izy (izy from http://www.izysoftware.com/) */
#if SDL_VERSION_ATLEAST(2,0,3)
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_NORMAL);
#endif
}

void Thread_SetThreadPriorityHigh(void)
{
	/* Thread Priority added by Izy (izy from http://www.izysoftware.com/) */
#if SDL_VERSION_ATLEAST(2,0,3)
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
#endif
}
