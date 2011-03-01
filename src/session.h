#ifndef SESSION_H
#define SESSION_H

#include <pthread.h>

#include <string>
#include <stdint.h>

#include <tcutil.h>

#include "constants.h"
#include "config.h"
#include "entity.h"

namespace Game
{
	//Session ID
	typedef uint64_t SessionID;
	
	//A player's session information
	struct Session
	{
		EntityID	player_entity;
	};

	//Session manager
	struct SessionManager
	{
		SessionManager();
		~SessionManager();
	};
};

#endif

