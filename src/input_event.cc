#include "input_event.h"

using namespace Game;


//Extracts an input event from the stream
int InputEvent::extract(
	void* buf, 
	size_t buf_len)
{
	if(buf_len < 1)
		return -1;
		
	uint8_t* ptr = (uint8_t*)buf;
	type = (InputEventType)*ptr;
	
	if(type == InputEventType::DigBlock)
	{
		if(buf_len < 4)
			return -1;
			
		dig_event.x = (int8_t)ptr[1];
		dig_event.y = (int8_t)ptr[2];
		dig_event.z = (int8_t)ptr[3];
		
		return 4;
	}
	else if(type == InputEventType::PlayerTick)
	{
		if(buf_len < 13)
			return -1;
			
		player_event.x = ptr[1] + (ptr[2]<<8) + (ptr[3]<<16);
		player_event.y = ptr[4] + (ptr[5]<<8) + (ptr[6]<<16);
		player_event.z = ptr[7] + (ptr[8]<<8) + (ptr[9]<<16);
		
		player_event.pitch = ptr[10];
		player_event.yaw = ptr[11];
		player_event.input_state = ptr[12];
		
		return 13;
	}
	else
	{
	}


	return -1;
}
