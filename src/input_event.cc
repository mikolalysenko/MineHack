#include <cstring>
#include <cassert>

#include "session.h"

#include "input_event.h"

using namespace Game;

template<typename T> void net_deserialize(uint8_t*& ptr, T& v)
{
	v = *((T*)(void*)ptr);
	ptr += sizeof(T);
}


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
	const int LEN = 8 + (4 + 4 + 4) + 3;

	uint8_t *ptr = (uint8_t*)buf;

	if(buf_len < LEN)
		return -1;
	
	net_deserialize(ptr, tick);
		
	//Unpack position value
	uint32_t ix, iy, iz;
	net_deserialize(ptr, ix);
	net_deserialize(ptr, iy);
	net_deserialize(ptr, iz);	
	x = ((double)ix) / COORD_NET_PRECISION;
	y = ((double)iy) / COORD_NET_PRECISION;
	z = ((double)iz) / COORD_NET_PRECISION;
	
	//Unpack pitch/yaw
	int ipitch, iyaw;
	ipitch = *(ptr++);
	iyaw = *(ptr++);
	pitch	= 2. * M_PI / 256. * (double)ipitch;
	yaw		= 2. * M_PI / 256. * (double)iyaw;
	
	//Unpack input
	input_state = *(ptr++);
	
	int res = (int)(ptr - (uint8_t*)buf);
	assert(res == LEN);
	return res;
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
