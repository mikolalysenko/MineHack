#ifndef INPUT_EVENT_H
#define INPUT_EVENT_H

#include <cstdio>
#include <string>
#include <cstdint>

#include "session.h"

#include "chunk.h"

namespace Game
{
	//Different input events
	enum class InputEventType : uint16_t
	{
		DigBlock	= 0,
		PlayerTick	= 1,
		PlaceBlock	= 2,
		
		//Internal event types, first 256 values used for network events
		PlayerJoin	= 0x100,
		PlayerLeave,
	};
	
	
	struct DigEvent
	{
		int x, y, z;
	};
		
	struct PlayerEvent
	{
		int x, y, z;
		float pitch, yaw;
		int input_state;
	};
	
	struct BlockEvent
	{
		int x, y, z;
		Block b;
	};

	struct JoinEvent
	{
		char name[32];
	};
	
	struct LeaveEvent
	{
	};
	
		
	struct InputEvent
	{
		InputEventType		type;
		Server::SessionID 	session_id;
		
		union
		{
			DigEvent	dig_event;
			JoinEvent	join_event;
			PlayerEvent player_event;
			LeaveEvent	leave_event;
			BlockEvent	block_event;
		};
		
		
		//Extracts an input event from the stream
		int extract(
			void* buf, 
			size_t buf_len);
	};
	
	
};

#endif

