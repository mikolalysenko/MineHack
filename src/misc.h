#ifndef MISC_H
#define MISC_H

#include <sstream>
#include <cstdlib>

#include <pthread.h>

#include <tcutil.h>
#include <tctdb.h>


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

struct SpinLock
{
	pthread_spinlock_t	*lock;
	
	SpinLock(pthread_spinlock_t *l) : lock(l) { pthread_spin_lock(lock); }
	~SpinLock() { pthread_spin_unlock(lock); }
};

struct ScopeFree
{
	void* ptr;
	
	ScopeFree(void* p) : ptr(p) {}
	~ScopeFree() { if(ptr) std::free(ptr); }
};

template<class T> struct ScopeDelete
{
	T* ptr;
	ScopeDelete(T* p) : ptr(p) {}
	~ScopeDelete() { if(ptr) delete ptr; }
};


struct ScopeTCQuery
{
	TDBQRY* query;
	
	ScopeTCQuery(TCTDB* db) { query = tctdbqrynew(db); }
	~ScopeTCQuery() { tctdbqrydel(query); }
	
};

template<typename T>
	void add_query_cond(ScopeTCQuery& Q, const char* name, int op, T v)
{
	std::stringstream ss;
	ss << v;
	tctdbqryaddcond(Q.query, name, op, ss.str().c_str());
}

struct ScopeTCList
{
	TCLIST*	list;
	
	ScopeTCList(TCLIST* l) : list(l) {}
	ScopeTCList(ScopeTCQuery& q) : list(tctdbqrysearch(q.query)) {}
	
	~ScopeTCList()
	{
		if(list) tclistdel(list);
	}
};

struct ScopeTCMap
{
	TCMAP* map;
	
	ScopeTCMap() : map(tcmapnew()) { }
	
	ScopeTCMap(TCMAP* m) : map(m) {}
	
	~ScopeTCMap()
	{
		if(map) tcmapdel(map);
	}
};

//A thread safe queue
// T must have a field called next with type T*
template<typename T> struct SpinQueue
{
	SpinQueue();
	~SpinQueue();
	
	T* pop()
	{
		T* res;
		pthread_spin_lock(&lock);
		res = head;
		if(res)
		{
			head = res->next;
			if(res == tail)
				tail = NULL;
		}
		pthread_spin_unlock(&lock);
		res->next = NULL;
		return res;
	}
	
	void push(T* event)
	{
		event->next = NULL;
		pthread_spin_lock(&lock);
		if(tail)
		{
			tail->next = event;
			tail = event;
		}
		else
		{
			head = event;
		}	
		pthread_spin_unlock(&lock);
	}
	
	bool empty()
	{
		return head == NULL;
	}

private:
	T *head, *tail;
	pthread_spinlock_t lock;
};
	


#endif
