#ifndef WEBSOCKET_QUEUE_H
#define WEBSOCKET_QUEUE_H

#include <cstdio>
#include <cassert>
#include <list>
#include <tbb/spin_mutex.h>
#include <google/protobuf/message.h>

namespace Game
{
	struct WebSocketQueue_Impl
	{
		WebSocketQueue_Impl() : ref_count(0) {}
		
		~WebSocketQueue_Impl()
		{
			printf("Destructing ws_queue %016lx\n", this);
			for(auto iter = msg_queue.begin(); iter != msg_queue.end(); ++iter)
			{
				if(*iter != NULL)
				{
					printf("Deleting %016lx\n", *iter);
					delete *iter;
				}
			}
			printf("Destruction complete\n");
		}
	
		int ref_count;	
		tbb::spin_mutex lock;
		std::list<google::protobuf::Message*>  msg_queue;
	};


	struct WebSocketQueue
	{
		WebSocketQueue() : impl(NULL) { }
	
		WebSocketQueue(WebSocketQueue_Impl* impl_) : impl(impl_)
		{
			tbb::spin_mutex::scoped_lock L(impl->lock);
			impl->ref_count++;
		}
		
		~WebSocketQueue()
		{
			if(impl != NULL)
			{
				tbb::spin_mutex::scoped_lock L(impl->lock);
				if(impl->ref_count-- == 1)
				{
					delete impl;
				}
			}
		}
		
		int ref_count()
		{
			if(impl != NULL)
				return impl->ref_count;
			return 0;
		}
		
		void attach(WebSocketQueue_Impl* impl_)
		{
			assert(impl == NULL);
			
			impl = impl_;
			tbb::spin_mutex::scoped_lock L(impl->lock);
			impl->ref_count++;
		}
		
		bool detach_and_check_empty()
		{
			if(impl == NULL)
				return false;
				
			tbb::spin_mutex::scoped_lock L(impl->lock);
			if((--impl->ref_count) == 0)
			{
				delete impl;
				return false;
			}
			
			bool res = impl->msg_queue.empty();
			impl = NULL;
			return res;
		}
		
		bool pop_if_full(google::protobuf::Message*& res)
		{
			if(impl == NULL)
				return false;	
			tbb::spin_mutex::scoped_lock L(impl->lock);
			
			if(impl->msg_queue.empty())
				return false;
				
			res = impl->msg_queue.front();
			impl->msg_queue.pop_front();
			return true;
		}
		
		
		bool push_and_check_empty(google::protobuf::Message* v)
		{
			if(impl == NULL)
				return false;
			tbb::spin_mutex::scoped_lock L(impl->lock);

			bool r = impl->msg_queue.empty();
			impl->msg_queue.push_back(v);
			return r;
		}
		
		
		bool pop_and_check_empty()
		{
			if(impl == NULL)
				return false;
			tbb::spin_mutex::scoped_lock L(impl->lock);
			
			impl->msg_queue.pop_front();
			return impl->msg_queue.empty();
		}
		
		bool peek_if_full(google::protobuf::Message*& res)
		{
			if(impl == NULL)
				return false;
			tbb::spin_mutex::scoped_lock L(impl->lock);
			
			if(impl->msg_queue.empty())
				return false;
			res = impl->msg_queue.front();
			return true;
		}
		
	private:
		WebSocketQueue_Impl*	impl;
	};

};

#endif
