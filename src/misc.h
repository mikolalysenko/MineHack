#ifndef MISC_H
#define MISC_H

#include <cstdlib>
#include <pthread.h>

#include "mongoose.h"


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

struct MutexLock
{
	pthread_mutex_t *lock;
	
	MutexLock(pthread_mutex_t *l) : lock(l) { pthread_mutex_lock(lock); }
	~MutexLock() { pthread_mutex_unlock(lock); }
};

struct ScopeFree
{
	void* ptr;
	
	ScopeFree(void* p) : ptr(p) {}
	ScopeFree() { std::free(ptr); }
};

template<class T> struct ScopeDelete
{
	T* ptr;
	ScopeDelete(T* p) : ptr(p) {}
	~ScopeDelete() { delete ptr; }
};


#endif
