#include <cstring>
#include <cassert>

#include "session.h"

#include "input_event.h"

using namespace Game;


int DigEvent::extract(void* buf, size_t buf_len)
{
	uint8_t *ptr = (uint8_t*)buf;

	if(buf_len < 12)
		return -1;
	
	x  = *(ptr++);
	x |= (*(ptr++)) <<  8;
	x |= (*(ptr++)) << 16;
	x |= (*(ptr++)) << 24;
	
	y  = *(ptr++);
	y |= (*(ptr++)) <<  8;
	y |= (*(ptr++)) << 16;
	y |= (*(ptr++)) << 24;
	
	z  = *(ptr++);
	z |= (*(ptr++)) <<  8;
	z |= (*(ptr++)) << 16;
	z |= (*(ptr++)) << 24;
	
	return 12;
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
	if(buf_len < len+1 || len > CHAT_LINE_MAX_LEN)
		return -1;
	
	//Zero out the chat buffer
	memset(msg, 0, CHAT_LINE_MAX_LEN);
	memcpy(msg, ptr, len);
	return len+1;
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
		
		default:
			return -1;
	}
		
	if(l < 0)
		return -1;
	return l+1;
}
