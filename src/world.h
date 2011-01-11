#ifndef GAME_H
#define GAME_H

#include <pthread.h>

#include <map>
#include <cstdint>

#include <Eigen/Core>

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
		
		//Adds an event to the server
		void add_event(InputEvent* ev);
		
		//Retrieves a compressed chunk from the server
		int get_compressed_chunk(
			Server::SessionID const&, 
			ChunkID const&,
			uint8_t* buf,
			size_t buf_len);
		
		//Sends queued messages to client
		int heartbeat(
			Server::SessionID const&,
			uint8_t* buf,
			size_t buf_len);
		
		//Ticks the server
		void tick();
		
	private:
		WorldGen	*gen;
		Map    		*game_map;		
		
		std::map<Server::SessionID, Player*> players;
		pthread_mutex_t players_lock;
		
		std::vector<InputEvent*> pending_events, events;
		pthread_mutex_t event_lock;
		
		void handle_add_player(Server::SessionID const&, JoinEvent const&);
		void handle_remove_player(Server::SessionID const&);
		void handle_set_block(Server::SessionID const&, BlockEvent const&);
		void handle_dig_block(Server::SessionID const&, DigEvent const&);

		//Broadcasts an update to all players in radius
		void broadcast_update(UpdateEvent * ev, double, double, double, double radius);

	};
};

#endif

