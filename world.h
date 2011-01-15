#ifndef GAME_H
#define GAME_H

#include <pthread.h>

#include <map>
#include <cstdint>
#include <cstdlib>

#include <tcutil.h>

#include "session.h"
#include "chunk.h"
#include "input_event.h"
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
		
		//Adds an event to the server
		void add_event(InputEvent const& ev);
		
		//Retrieves a compressed chunk from the server
		int get_compressed_chunk(
			Server::SessionID const&, 
			ChunkID const&,
			uint8_t* buf,
			size_t buf_len);
		
		//Sends queued messages to client
		void* heartbeat(
			Server::SessionID const&,
			int& len);
		
		//Ticks the server
		void tick();
		
	private:
		Map    			*game_map;
		
		//Mailbox for player updates
		UpdateMailbox	player_updates;
		
		//Input event queue and event lock
		std::vector<InputEvent> pending_events, events;
		pthread_mutex_t event_lock;

		
		//While held, the world is updated
		pthread_mutex_t world_lock;
		//TODO: Make this more fine grained later on
		
		//Player database
		std::map<Server::SessionID, Player*> players;
		
		
		void handle_add_player(Server::SessionID const&, JoinEvent const&);
		void handle_remove_player(Player*);
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

