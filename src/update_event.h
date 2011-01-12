#ifndef UPDATE_EVENT_H
#define UPDATE_EVENT_H

#include <string>

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
		
		int write(void* buf, size_t len) const;
	};
	
	struct UpdateChatEvent
	{
		uint8_t name_len;
		char name[USERNAME_MAX_LEN];
		
		uint8_t msg_len;
		char msg[CHAT_LINE_MAX_LEN];

		int write(void* buf, size_t len) const;
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
		int write(void* buf, size_t len) const;
	};
};

#endif

