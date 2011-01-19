#include <pthread.h>

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


int UpdateEntityEvent::write(uint8_t* buf) const
{
	uint8_t* ptr = buf;
	
	//Serialize entity header
	*(ptr++) = (uint8_t) UpdateEventType::UpdateEntity;
	*((EntityID*)ptr) = entity.entity_id;
	ptr += sizeof(EntityID);
	
	//Write length of packet
	int len = 0;
	
	//TODO: Serialize entity base
	
	
	//Write the data out for the length
	*(ptr++) = len & 0xff;
	*(ptr++) = (len >> 8) & 0xff;
	
	return len + 11;
}

int UpdateDeleteEvent::write(uint8_t* buf) const
{
	uint8_t* ptr = buf;
	
	*(ptr++) = (uint8_t) UpdateEventType::DeleteEntity;
	*((EntityID*)ptr) = entity_id;
	ptr += sizeof(EntityID);
	
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
		
		default:
			return -1;
	}
}

UpdateMailbox::UpdateMailbox()
{
	pthread_spin_init(&lock, NULL);
	
	//Create the mailboxes
	mailboxes = tcmapnew2(256);
}

UpdateMailbox::~UpdateMailbox()
{
	tcmapdel(mailboxes);
}
	
//Sends an event to a terget
void UpdateMailbox::send_event(EntityID const& player_id, UpdateEvent const& up)
{
	uint8_t data[sizeof(UpdateEvent)];
	int len = up.write(data);
	if(len <= 0)
		return;
	
	//Lock database and add key	
	{	SpinLock	guard(&lock);
		tcmapputcat(mailboxes, &player_id, sizeof(EntityID), data, len);
	}
}
		
//Retrieves all events
void* UpdateMailbox::get_events(EntityID const& player_id, int& len)
{
	void * result = NULL;
	
	{	SpinLock	guard(&lock);
		
		const void* data = tcmapget(mailboxes, &player_id, sizeof(EntityID), &len);
			
		if(data == NULL)
			return NULL;
			
		//Copy data into result
		result = malloc(len);
		memcpy(result, data, len);
		
		tcmapout(mailboxes, &player_id, sizeof(EntityID));
	}
	
	return result;
}

//Sends an event to a terget
void UpdateMailbox::clear_events(EntityID const& player_id)
{
	SpinLock	guard(&lock);
	tcmapout(mailboxes, &player_id, sizeof(EntityID));
}


