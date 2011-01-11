#ifndef UPDATE_EVENT_H
#define UPDATE_EVENT_H

#include <string>

#include "chunk.h"

namespace Game
{
	enum class UpdateEventType
	{
		SetBlock,
		Chat
	};
	
	struct UpdateBlockEvent
	{
		int x, y, z;
		Block b;
	};
	
	struct UpdateEvent
	{
		UpdateEventType type;
		
		union
		{
			UpdateBlockEvent	block_event;
		};
		
		//Writes output event to buffer
		int write(void* buf, size_t len);
	};
};

#endif

