#include <pthread.h>

#include <cmath>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <cstdint>

#include <tcutil.h>

#include "session.h"
#include "misc.h"
#include "update_event.h"

using namespace std;
using namespace Server;
using namespace Game;

int UpdateBlockEvent::write(uint8_t* buf) const
{
	uint8_t* ptr = buf;
	
	*(ptr++) = (uint8_t)UpdateEventType::SetBlock;
	*(ptr++) = (uint8_t)b;
	*(ptr++) =  x 	  	& 0xff;
	*(ptr++) = (x >> 8) & 0xff;
	*(ptr++) = (x >> 16)& 0xff;
	*(ptr++) = (x >> 24)& 0xff;
	*(ptr++) =  y 	  	& 0xff;
	*(ptr++) = (y >> 8) & 0xff;
	*(ptr++) = (y >> 16)& 0xff;
	*(ptr++) = (y >> 24)& 0xff;
	*(ptr++) =  z 	  	& 0xff;
	*(ptr++) = (z >> 8) & 0xff;
	*(ptr++) = (z >> 16)& 0xff;
	*(ptr++) = (z >> 24)& 0xff;
	
	return 14;
}

int UpdateChatEvent::write(uint8_t* buf) const
{
	assert(name_len <= USER_NAME_MAX_LEN);
	assert(msg_len <= CHAT_LINE_MAX_LEN);
	
	int len = 3 + name_len + msg_len;

	uint8_t* ptr = buf;
	
	//Set type
	*(ptr++) = (uint8_t) UpdateEventType::Chat;
	
	*(ptr++) = name_len;
	memcpy(ptr, name, name_len);
	ptr += name_len;

	*(ptr++) = msg_len;
	memcpy(ptr, msg, msg_len);
	
	return len;
}


template<typename T> void net_serialize(uint8_t*& ptr, T v)
{
	*((T*)(void*)ptr) = v;
	ptr += sizeof(T);
}

//Serialize the base state for an entity
int serialize_entity(Entity const& entity, uint8_t* buf, bool initialize)
{
	if(!entity.base.active)
		return 0;
	
	
	uint8_t* ptr = buf;
	
	*(ptr++) = (uint8_t)entity.base.type;
	
	//TODO: Check if entity is physical, if so need to replicate position
	net_serialize(ptr, (uint32_t)(entity.base.x * COORD_NET_PRECISION));
	net_serialize(ptr, (uint32_t)(entity.base.y * COORD_NET_PRECISION));
	net_serialize(ptr, (uint32_t)(entity.base.z * COORD_NET_PRECISION));
	
	*(ptr++) = (uint8_t)((int)(entity.base.pitch * 256. / (2. * M_PI) + (1<<16)) & 0xff);
	*(ptr++) = (uint8_t)((int)(entity.base.yaw * 256. / (2. * M_PI) + (1<<16)) & 0xff);
	*(ptr++) = (uint8_t)((int)(entity.base.roll * 256. / (2. * M_PI) + (1<<16)) & 0xff);
	
	//TODO: Serialize other relevant data
	if(entity.base.type == EntityType::Player && initialize)
	{
		uint8_t* name_len = ptr++;
		
		for(int i=0; i<20; i++)
		{
			char c = entity.player.player_name[i];
			if(c == '\0')
			{
				*name_len = i;
				break;
			}
			*(ptr++) = c;
		}
	}
	
	
	//Return length of written region
	return (int)(ptr - buf);
}


int UpdateEntityEvent::write(uint8_t* buf) const
{
	uint8_t* ptr = buf;
	
	//Serialize entity header
	*(ptr++) = (uint8_t) UpdateEventType::UpdateEntity;
	net_serialize(ptr, entity.entity_id);
	
	//Write length of packet
	uint16_t* len_ptr = (uint16_t*)(void*)ptr;
	ptr += sizeof(uint16_t);
	
	int len = serialize_entity(entity, ptr, initialize);
	ptr += len;	
	*len_ptr = (uint16_t)len;
	
	return (int)(ptr - buf);
}

int UpdateDeleteEvent::write(uint8_t* buf) const
{
	uint8_t* ptr = buf;
	
	*(ptr++) = (uint8_t) UpdateEventType::DeleteEntity;
	net_serialize(ptr, entity_id);
	
	return (int)(ptr - buf);
}


int UpdateClockEvent::write(uint8_t* buf) const
{
	uint8_t* ptr = buf;
	
	*(ptr++) = (uint8_t) UpdateEventType::SyncClock;
	net_serialize(ptr, tick_count);

	return (int)(ptr - buf);
}


//Update event serialization
int UpdateEvent::write(uint8_t* buf) const
{
	switch(type)
	{
		case UpdateEventType::SetBlock:
			return block_event.write(buf);
		
		case UpdateEventType::Chat:
			return chat_event.write(buf);
		
		case UpdateEventType::UpdateEntity:
			return entity_event.write(buf);
		
		case UpdateEventType::DeleteEntity:
			return delete_event.write(buf);
		
		case UpdateEventType::SyncClock:
			return clock_event.write(buf);

		
		default:
			return -1;
	}
}


