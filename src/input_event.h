#ifndef INPUT_EVENT_H
#define INPUT_EVENT_H

#include <cstdio>
#include <string>
#include <cstdint>

#include "constants.h"
#include "chunk.h"

namespace Game
{
	//Different input events
	enum class InputEventType : uint16_t
	{
		NoEvent 	= 0,
	
		PlayerTick	= 1,
		DigBlock	= 2,
		PlaceBlock	= 3,
		Chat		= 4
	};
	
	
	struct DigEvent
	{
		int x, y, z;
		
		int extract(void* buf, size_t len);
	};
		
	struct PlayerEvent
	{
		uint64_t	tick;
		double 		x, y, z;
		float 		pitch, yaw, roll;
		int 		input_state;

		int extract(void* buf, size_t len);
	};
	
	struct BlockEvent
	{
		int x, y, z;
		Block b;
		
		int extract(void* buf, size_t len);
	};

	struct ChatEvent
	{
		uint8_t len;
		char msg[CHAT_LINE_MAX_LEN + 1];
		
		int extract(void* buf, size_t len);
	};
		
	struct InputEvent
	{
		InputEventType		type;
		
		union
		{
			DigEvent	dig_event;
			PlayerEvent player_event;
			BlockEvent	block_event;
			ChatEvent	chat_event;
		};
		
		
		//Extracts an input event from the stream
		int extract(
			void* buf, 
			size_t buf_len);
	};
	
	
};

#endif

