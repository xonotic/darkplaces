#ifndef THREAD_H
#define THREAD_H

// disable threads
//#define THREADDISABLE

// use recursive mutex (non-posix) extensions in thread_pthread
#define THREADRECURSIVE

int Thread_Init(void);
void Thread_Shutdown(void);
qboolean Thread_HasThreads(void);
void *Thread_CreateMutex(void);
void Thread_DestroyMutex(void *mutex);
int Thread_LockMutex(void *mutex);
int Thread_UnlockMutex(void *mutex);
void *Thread_CreateCond(void);
void Thread_DestroyCond(void *cond);
int Thread_CondSignal(void *cond);
int Thread_CondBroadcast(void *cond);
int Thread_CondWait(void *cond, void *mutex);
void *Thread_CreateThread(int (*fn)(void *), void *data);
void Thread_WaitThread(void *thread, int *retval);
void *Thread_CreateBarrier(unsigned int count);
void Thread_DestroyBarrier(void *barrier);
void Thread_WaitBarrier(void *barrier);
void Thread_SetThreadPriorityLow(void);
void Thread_SetThreadPriorityNormal(void);
void Thread_SetThreadPriorityHigh(void);

#endif
