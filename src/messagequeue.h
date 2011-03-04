#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <queue>
#include <tbb/spin_mutex.h>

namespace Game
{
	//A simplistic message queue data structure
	template<typename T> struct MessageQueue
	{
		//Adds an item to the queue, returns true if the new item is the first item in the queue
		bool push(T const& v)
		{
			tbb::spin_mutex::scoped_lock L(lock);
			bool r = lst.empty();
			lst.push(v);
			return r;
		}
	
		//Removes last item, returns true if there are still more items in the queue
		bool pop()
		{
			tbb::spin_mutex::scoped_lock L(lock);
			lst.pop();
			return lst.empty();
		}
		
		//Peeks at the top message
		bool front(T& r)
		{
			tbb::spin_mutex::scoped_lock L(lock);
			if(lst.empty())
				return false;
			r = lst.front();
			return true;
		}
		
		
	private:
	
		tbb::spin_mutex lock;
		std::queue<T> 	lst;
	};
};

#endif

