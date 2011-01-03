#ifndef MISC_H
#define MISC_H

#include <pthread.h>

struct ReadLock
{
	pthread_rwlock_t *lock;
	
	ReadLock(pthread_rwlock_t *l) : lock(l)  { pthread_rwlock_rdlock(lock); }
	~ReadLock() { pthread_rwlock_unlock(lock); }
};

struct WriteLock
{
	pthread_rwlock_t *lock;
	
	WriteLock(pthread_rwlock_t *l) : lock(l)  { pthread_rwlock_wrlock(lock); }
	~WriteLock() { pthread_rwlock_unlock(lock); }
};



#endif
