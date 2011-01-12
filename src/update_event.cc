#include <cstring>
#include <cassert>

#include "update_event.h"

using namespace std;
using namespace Game;

int UpdateBlockEvent::write(void* bufv, size_t buf_len) const
{
	if(buf_len < 10)
		return -1;
		
	uint8_t* buf = (uint8_t*)bufv;
			
	buf[0] = (uint8_t)b;
	buf[1] =  x 	  & 0xff;
	buf[2] = (x >> 8) & 0xff;
	buf[3] = (x >> 16)& 0xff;
	buf[4] =  y 	  & 0xff;
	buf[5] = (y >> 8) & 0xff;
	buf[6] = (y >> 16)& 0xff;
	buf[7] =  z 	  & 0xff;
	buf[8] = (z >> 8) & 0xff;
	buf[9] = (z >> 16)& 0xff;
	
	return 10;
}

int UpdateChatEvent::write(void* bufv, size_t buf_len) const
{
	if(buf_len < name_len + msg_len + 2)
		return -1;
		
	uint8_t* ptr = (uint8_t*)bufv;
	
	assert(name_len < 20);
	*(ptr++) = name_len;
	memcpy(ptr, name, name_len);
	ptr += name_len;

	*(ptr++) = msg_len;
	memcpy(ptr, msg, msg_len);
	
	return name_len + msg_len + 2;
}



//Update event serialization
int UpdateEvent::write(void* bufv, size_t buf_len) const
{
	uint8_t* buf = (uint8_t*)bufv;
	
	//Write the type byte
	if(buf_len < 1)
		return -1;
	*(buf++) = (uint8_t)type;
	buf_len--;
	
	int r = -1;
	
	switch(type)
	{
		case UpdateEventType::SetBlock:
			r = block_event.write(buf, buf_len);
		break;
		
		case UpdateEventType::Chat:
			r = chat_event.write(buf, buf_len);
		break;
	}
	
	if(r < 0)
		return -1;
	return r+1;
}
