#ifndef INPUT_EVENT_H
#define INPUT_EVENT_H

#include <cstdio>

#include "session.h"

#include "chunk.h"

namespace Game
{
	//Different input events
	enum class InputEventType
	{
		PlayerJoin,
		PlayerLeave,
		SetBlock
	};
	
	struct JoinEvent
	{
		char name[32];
	};
	
	struct LeaveEvent
	{
	};
	
	struct BlockEvent
	{
		int x, y, z;
		Block b;
	};
	
	struct InputEvent
	{
		InputEventType		type;
		Server::SessionID 	session_id;
		
		union
		{
			JoinEvent	join_event;
			LeaveEvent	leave_event;
			BlockEvent	block_event;
		};
	};
};

#endif

