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

namespace Game
{
	struct World
	{
		//World data
		bool		running;
						
		//Ctor
		World();
		~World();
		
		//Adds a player
		bool add_player(std::string const& player_name);
		
		//Removes a player
		bool remove_player(std::string const& player_name);
		
		//Adds an event to the server
		void add_event(std::string const& player_name, InputEvent const& ev);
		
		//Retrieves a compressed chunk from the server
		int get_compressed_chunk(
			std::string const& player_name,
			ChunkID const&,
			uint8_t* buf,
			size_t buf_len);
		
		//Sends queued messages to client
		void* heartbeat(
			std::string const& player_name,
			int& len);
		
		//Ticks the server
		void tick();
		
	private:
		Map    			*game_map;
		
		//Event queues
		std::vector< std::pair<std::string, InputEvent> > pending_events, events;
		pthread_mutex_t		event_lock;
		
		//Mailbox for player updates
		UpdateMailbox	player_updates;
		
		//Player database
		std::map<std::string, Player*> players;
		pthread_mutex_t		player_lock;
		
		
		void handle_place_block(Player*, BlockEvent const&);
		void handle_dig_block(Player*, DigEvent const&);
		void handle_player_tick(Player*, PlayerEvent const&);
		void handle_chat(Player*, ChatEvent const&);

		//Broadcasts an update to all players in radius
		void broadcast_update(UpdateEvent const&, 
			double x, double y, double z, double r);

	};
};

#endif

