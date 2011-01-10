#ifndef GAME_H
#define GAME_H

#include <map>
#include <cstdint>

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
		WorldGen	*gen;
		Map    		*game_map;
		
		//The player database
		std::map<Server::SessionID, Player*> players;
				
		World();
		
		//Adds an event to the server
		void add_event(const InputEvent& ev);
		
		//Retrieves a compressed chunk from the server
		int get_compressed_chunk(
			const Server::SessionID&, 
			const ChunkId&,
			void* buf,
			size_t buf_len);
		
		//Sends queued messages to client
		int heartbeat(
			const Server::SessionID&,
			void* buf,
			size_t buf_len);
			
		//Ticks the server
		void tick();
	};
};

#endif

