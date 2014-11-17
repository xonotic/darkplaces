#include "quakedef.h"
#include "thread.h"

int Thread_Init(void)
{
	return 0;
}

void Thread_Shutdown(void)
{
}

qboolean Thread_HasThreads(void)
{
	return false;
}

void *Thread_CreateMutex(void)
{
	return NULL;
}

void Thread_DestroyMutex(void *mutex)
{
}

int Thread_LockMutex(void *mutex)
{
	return -1;
}

int Thread_UnlockMutex(void *mutex)
{
	return -1;
}

void *Thread_CreateCond(void)
{
	return NULL;
}

void Thread_DestroyCond(void *cond)
{
}

int Thread_CondSignal(void *cond)
{
	return -1;
}

int Thread_CondBroadcast(void *cond)
{
	return -1;
}

int Thread_CondWait(void *cond, void *mutex)
{
	return -1;
}

void *Thread_CreateThread(int (*fn)(void *), void *data)
{
	return NULL;
}

void Thread_WaitThread(void *thread, int *retval)
{
}

void *Thread_CreateBarrier(unsigned int count)
{
	return NULL;
}

void Thread_DestroyBarrier(void *barrier)
{
}

void Thread_WaitBarrier(void *barrier)
{
}

void Thread_SetThreadPriorityLow(void)
{
	/* Thread Priority added by Izy (izy from http://www.izysoftware.com/) */
}

void Thread_SetThreadPriorityNormal(void)
{
	/* Thread Priority added by Izy (izy from http://www.izysoftware.com/) */
}

void Thread_SetThreadPriorityHigh(void)
{
	/* Thread Priority added by Izy (izy from http://www.izysoftware.com/) */
}
