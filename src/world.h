#ifndef GAME_H
#define GAME_H

#include <pthread.h>

#include <utility>
#include <string>
#include <map>
#include <cstdint>
#include <cstdlib>

#include <tcutil.h>

#include "constants.h"
#include "config.h"
#include "chunk.h"
#include "entity.h"
#include "worldgen.h"
#include "map.h"
#include "player.h"

namespace Game
{
	class World
	{
	public:
		//State variables
		bool			running;
		uint64_t		tick_count;
						
		//Ctor
		World();
		~World();
		
		//Player management functions
		bool player_create(std::string const& player_name, EntityID& player_id);
		bool get_player_entity(std::string const& player_name, EntityID& player_id);
		void player_delete(EntityID const& player_id);
		bool player_join(EntityID const& player_id);
		bool player_leave(EntityID const& player_id);
		
		//Input handlers
		void handle_input(Network::ClientInput* input, int socket);
			
		//Retrieves all visible chunks around player
		bool get_vis_chunks(EntityID const&, int socket);
		
		//Ticks the server
		void tick();
		
	private:
	
		//Various systems
		WorldGen		*world_gen;
		Map    			*game_map;
		EntityDB		*entity_db;
		PlayerSet		*players;
		Config			*config;
	};
};

#endif

