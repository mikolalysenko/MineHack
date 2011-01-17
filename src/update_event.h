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
		DestroyEntity	= 4,
	};
	
	struct UpdateBlockEvent
	{
		int x, y, z;
		Block b;
		
		void* write(int& len) const;
	};
	
	struct UpdateChatEvent
	{
		uint8_t name_len;
		char name[USER_NAME_MAX_LEN];
		
		uint8_t msg_len;
		char msg[CHAT_LINE_MAX_LEN];

		void* write(int& len) const;
	};
	
	//Creates/updates an entity on the client
	struct UpdateEntityEvent
	{
		Entity	entity;
		
		void* write(int& len) const;
	};
	
	//Destroys an entity on the client
	struct UpdateDestroyEntityEvent
	{
		EntityID	entity_id;
		
		void* write(int& len) const;
	};
	
	struct UpdateEvent
	{
		UpdateEventType type;
		
		union
		{
			UpdateBlockEvent			block_event;
			UpdateChatEvent				chat_event;
			UpdateEntityEvent			entity_event;
			UpdateDestroyEntityEvent	destroy_entity_event;
		};
		
		//Writes output event to buffer
		//Client must release pointer with free
		void* write(int& len) const;
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

