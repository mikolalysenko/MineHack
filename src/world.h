#ifndef GAME_H
#define GAME_H

#include <pthread.h>

#include <utility>
#include <string>
#include <map>
#include <cstdint>
#include <cstdlib>

#include <tcutil.h>

#include "chunk.h"
#include "input_event.h"
#include "update_event.h"
#include "worldgen.h"
#include "map.h"
#include "player.h"
#include "config.h"


namespace Game
{
	struct World
	{
		//World data
		bool		running;
						
		//Ctor
		World();
		~World();
		
		//Player management events
		bool player_create(std::string const& player_name);
		void player_delete(std::string const& player_name);
		bool player_join(std::string const& player_name);
		bool player_leave(std::string const& player_name);
		
		//Adds an event to the server
		void handle_input(
			std::string const& player_name, 
			InputEvent const& ev);
		
		//Retrieves a compressed chunk from the server
		int get_compressed_chunk(
			std::string const& player_name,
			ChunkID const&,
			uint8_t* buf,
			size_t buf_len);
		
		//Processes queued messages for a particular client
		void* heartbeat(
			std::string const& player_name,
			int& len);
		
		//Ticks the server
		void tick();
		
	private:
	
		//The game's current tick count
		uint64_t		tick_count;
	
		//World generator
		WorldGen		*world_gen;
	
		//The game map
		Map    			*game_map;
		
		//Entity database
		EntityDB		*entity_db;

		//Player database
		PlayerDB		*player_db;

		//Mailbox for player updates
		UpdateMailbox	player_updates;
		
		Config			*config;
		
		//Broadcasts an update to all players in radius
		void broadcast_update(
			UpdateEvent const&, 
			Region const& r);
	};
};

#endif

