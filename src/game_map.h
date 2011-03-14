#ifndef GAME_MAP_H
#define GAME_MAP_H

#include <stdint.h>
#include <cstdlib>

#include <tbb/concurrent_hash_map.h>

#include "constants.h"
#include "config.h"
#include "chunk.h"
#include "worldgen.h"

namespace Game
{
	//This is basically a data structure which implements a caching/indexing system for chunks
	struct GameMap
	{
		//Map constructor
		GameMap(Config* config);
		~GameMap();
		
		//Retrieves a chunk
		void get_chunk(
			ChunkID const&, 
			Block* buffer, 
			int stride_x = CHUNK_X, 
			int stride_xy = CHUNK_X * CHUNK_Y);

		//Retrieves a raw buffer associated with the given chunk
		Network::Chunk* get_net_chunk();
		
	private:
		//The world generator and config stuff
		WorldGen* world_gen;
		Config* config;
		
		//The game map
		typedef tbb::concurrent_hash_map<ChunkID, ChunkBuffer, ChunkIDHashCompare> chunk_map_t;
		chunk_map_t chunks;
	};
	
};


#endif

