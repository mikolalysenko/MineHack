#ifndef UPDATE_EVENT_H
#define UPDATE_EVENT_H

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

namespace Game
{
	enum class UpdateEventType
	{
		SetBlock		= 1,
		Chat	 		= 2,
		UpdateEntity	= 3,
		DeleteEntity	= 4,
	};
	
	struct UpdateBlockEvent
	{
		int x, y, z;
		Block b;
		
		int write(uint8_t*) const;
	};
	
	struct UpdateChatEvent
	{
		uint8_t name_len;
		char name[PLAYER_NAME_MAX_LEN + 1];
		
		uint8_t msg_len;
		char msg[CHAT_LINE_MAX_LEN + 1];

		int write(uint8_t*) const;
	};
	
	//Creates/updates an entity on the client
	struct UpdateEntityEvent
	{
		Entity	entity;
		
		int write(uint8_t*) const;
	};
	
	//Destroys an entity on the client
	struct UpdateDeleteEvent
	{
		EntityID	entity_id;
		
		int write(uint8_t*) const;
	};
	
	struct UpdateEvent
	{
		UpdateEventType type;
		
		union
		{
			UpdateBlockEvent			block_event;
			UpdateChatEvent				chat_event;
			UpdateEntityEvent			entity_event;
			UpdateDeleteEvent			delete_event;
		};
		
		//Writes output event to buffer
		//Client must release pointer with free
		int write(uint8_t*) const;
	};
	
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

