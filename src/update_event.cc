#include "update_event.h"

using namespace Game;

//Update event serialization
int UpdateEvent::write(void* bufv, size_t buf_len)
{
	uint8_t* buf = (uint8_t*)bufv;
	int l = 0;
	
	if(type == UpdateEventType::SetBlock)
	{
		if(buf_len < 11)
			return -1;
				
		buf[l++] = 0;
		buf[l++] = (uint8_t)block_event.b;
		buf[l++] =  block_event.x 		& 0xff;
		buf[l++] = (block_event.x >> 8) & 0xff;
		buf[l++] = (block_event.x >> 16)& 0xff;
		buf[l++] =  block_event.y 		& 0xff;
		buf[l++] = (block_event.y >> 8) & 0xff;
		buf[l++] = (block_event.y >> 16)& 0xff;
		buf[l++] =  block_event.z 		& 0xff;
		buf[l++] = (block_event.z >> 8) & 0xff;
		buf[l++] = (block_event.z >> 16)& 0xff;
		
		return 11;
	}
	else
	{
	}

	return -1;
}
