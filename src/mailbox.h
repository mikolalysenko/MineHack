#ifndef MAILBOX_H
#define MAILBOX_H

#include <pthread.h>

#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstdint>

#include <tcutil.h>

#include "login.h"
#include "chunk.h"
#include "input_event.h"
#include "entity.h"
#include "update_event.h"

namespace Game
{
	struct UpdateMailbox
	{
		UpdateMailbox();
		~UpdateMailbox();
	
		//Sends an event to a terget
		void send_event(EntityID const& target, UpdateEvent const&);
		
		//Retrieves all events
		void* get_events(EntityID const& target, int& len);
		
		//Clears all pending events for target entity
		void clear_events(EntityID const& target);
		
	private:
		
		pthread_spinlock_t	lock;
		TCMAP*				mailboxes;
	};
};


#endif

