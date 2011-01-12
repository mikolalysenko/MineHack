#include <cstring>
#include <cassert>
#include "input_event.h"

using namespace Game;


int DigEvent::extract(void* buf, size_t buf_len)
{
	uint8_t *ptr = (uint8_t*)buf;

	if(buf_len < 9)
		return -1;
	
	x = ptr[0] + (ptr[1]<<8) + (ptr[2]<<16);
	y = ptr[3] + (ptr[4]<<8) + (ptr[5]<<16);
	z = ptr[6] + (ptr[7]<<8) + (ptr[8]<<16);
	
	return 9;
}


int PlayerEvent::extract(void* buf, size_t buf_len)
{
	uint8_t *ptr = (uint8_t*)buf;

	if(buf_len < 12)
		return -1;
		
	x = ptr[0] + (ptr[1]<<8) + (ptr[2]<<16);
	y = ptr[3] + (ptr[4]<<8) + (ptr[5]<<16);
	z = ptr[6] + (ptr[7]<<8) + (ptr[8]<<16);
	
	pitch = ptr[9];
	yaw = ptr[10];
	input_state = ptr[11];
	
	return 12;
}

int BlockEvent::extract(void* buf, size_t buf_len)
{
	//TODO: Not implemented
	return 0;
}


//Extracts a chat event from the stream
int ChatEvent::extract(void* buf, size_t buf_len)
{
	if(buf_len < 1)
		return -1;

	uint8_t* ptr = (uint8_t*)buf;
	
	len = *(ptr++);
	if(buf_len < len+1 || len > 128)
		return -1;
	
	memcpy(msg, ptr, len);
	return len+1;
}


int JoinEvent::extract(void* buf, size_t buf_len)
{
	assert(false);	//not network serializable
	return -1;
}

int LeaveEvent::extract(void* buf, size_t buf_len)
{
	assert(false); //not network serializable
	return -1;
}


//Extracts an input event from the stream
int InputEvent::extract(
	void* buf, 
	size_t buf_len)
{
	if(buf_len < 1)
		return -1;
		
	uint8_t* ptr = (uint8_t*)buf;
	type = (InputEventType)*(ptr++);
	buf_len--;
	
	int l = -1;
	
	//Dispatch block deserialization routine
	switch(type)
	{
		case InputEventType::PlayerTick:
			l = player_event.extract(ptr, buf_len);
		break;

		case InputEventType::DigBlock:
			l = dig_event.extract(ptr, buf_len);
		break;

		case InputEventType::PlaceBlock:
			l = block_event.extract(ptr, buf_len);
		break;
		
		case InputEventType::Chat:
			l = chat_event.extract(ptr, buf_len);
		break;
		
		case InputEventType::PlayerJoin:
			l = join_event.extract(ptr, buf_len);
		break;
		
		case InputEventType::PlayerLeave:
			l = leave_event.extract(ptr, buf_len);
		break;
		
		default:
			return -1;
	}
		
	if(l < 0)
		return -1;
	return l+1;
}
