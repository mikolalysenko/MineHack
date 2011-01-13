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

namespace Game
{
	enum class UpdateEventType
	{
		SetBlock = 1,
		Chat	 = 2
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
		char name[USERNAME_MAX_LEN];
		
		uint8_t msg_len;
		char msg[CHAT_LINE_MAX_LEN];

		void* write(int& len) const;
	};
	
	struct UpdateEvent
	{
		UpdateEventType type;
		
		union
		{
			UpdateBlockEvent	block_event;
			UpdateChatEvent		chat_event;
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
		void send_event(Server::SessionID const&, UpdateEvent const&);
		
		//Retrieves all events
		void* get_events(Server::SessionID const&, int& len);
		
	private:
		
		pthread_spinlock_t	lock;
		TCMAP*				mailboxes;
	};
};

#endif

