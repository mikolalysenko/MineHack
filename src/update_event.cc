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

void* UpdateBlockEvent::write(int& len) const
{
	len = 14;

	uint8_t* buf = (uint8_t*)malloc(len);
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
	
	return (void*)buf;
}

void* UpdateChatEvent::write(int& len) const
{
	assert(name_len <= USER_NAME_MAX_LEN);
	assert(msg_len <= CHAT_LINE_MAX_LEN);
	
	len = 3 + name_len + msg_len;

	uint8_t* buf = (uint8_t*)malloc(len);
	uint8_t* ptr = buf;
	
	//Set type
	*(ptr++) = (uint8_t) UpdateEventType::Chat;
	
	*(ptr++) = name_len;
	memcpy(ptr, name, name_len);
	ptr += name_len;

	*(ptr++) = msg_len;
	memcpy(ptr, msg, msg_len);
	
	return (void*)buf;
}


//Update event serialization
void* UpdateEvent::write(int& len) const
{
	switch(type)
	{
		case UpdateEventType::SetBlock:
			return block_event.write(len);
		break;
		
		case UpdateEventType::Chat:
			return chat_event.write(len);
		break;
		
		default:
			return NULL;
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
	int len;
	ScopeFree G(up.write(len));
	if(G.ptr == NULL)
		return;
	
	//Lock database and add key	
	{	SpinLock	guard(&lock);
		tcmapputcat(mailboxes, &player_id, sizeof(EntityID), G.ptr, len);
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

