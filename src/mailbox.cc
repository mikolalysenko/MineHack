
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
#include "mailbox.h"

using namespace std;
using namespace Server;
using namespace Game;


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


