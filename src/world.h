#ifndef WORLD_H
#define WORLD_H

#include <pthread.h>

#include <string>
#include <stdint.h>

#include <tcutil.h>

#include "constants.h"
#include "config.h"
#include "entity.h"
#include "session.h"

namespace Game
{
	class World
	{
	public:
						
		//Ctor
		World(Config* config);
		~World();
		
		//Event loop management
		bool start();
		void stop();
		void sync();
		
		//Player management functions
		bool player_create(std::string const& player_name);
		void player_delete(std::string const& player_name);
		bool player_join(std::string const& player_name, SessionID const& session_id);
		void player_leave(SessionID const& session_id);
		
	private:

		//State variables
		bool			running;
		uint64_t		ticks;
	
		//Subsystems
		Config			*config;
		SessionManager	*session_manager;
		EntityDB		*entity_db;
	};
};

#endif

