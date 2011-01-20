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
#include "chunk.h"
#include "entity.h"
#include "entity_db.h"
#include "input_event.h"
#include "update_event.h"
#include "mailbox.h"
#include "worldgen.h"
#include "map.h"
#include "config.h"

namespace Game
{
	struct World
	{
		//World data
		bool		running;
		
		//The game's current tick count
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
		
		//Adds an event to the server
		void handle_input(
			EntityID const&, 
			InputEvent const& ev);
		
		//Retrieves a compressed chunk from the server
		int get_compressed_chunk(
			EntityID const&,
			ChunkID const&,
			uint8_t* buf,
			size_t buf_len);
		
		//Processes queued messages for a particular client
		void* heartbeat(
			EntityID const&,
			int&);
		
		//Ticks the server
		void tick();
		
	private:
	
		//World generator
		WorldGen		*world_gen;
	
		//The game map
		Map    			*game_map;
		
		//Entity database
		EntityDB		*entity_db;

		//Mailbox for player updates
		UpdateMailbox	player_updates;
		
		//Configuration stuff
		Config			*config;
		
		//Input handlers
		void handle_player_tick(EntityID const& player_id, PlayerEvent const& input);
		void handle_chat(EntityID const& player_id, ChatEvent const& input);
		
		//Broadcasts an update to all players in radius
		void broadcast_update(
			UpdateEvent const&, 
			Region const& r);
			
		//Sends a resynchronize packet to the target player
		void resync_player(EntityID const&);
		
		//Pushes updates for all entities in a particular region to target player
		void get_entity_updates(EntityID const& player_id, Region const& region, bool initialize);
	};
};

#endif

