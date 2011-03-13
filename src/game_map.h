#ifndef GAME_MAP_H
#define GAME_MAP_H

#include <stdint.h>
#include <cstdlib>

#include <tbb/concurrent_hash_map.h>

#include "constants.h"
#include "chunk.h"
#include "config.h"
#include "worldgen.h"

namespace Game
{
	//This is basically a data structure which implements a caching/indexing system for chunks
	struct GameMap
	{
		//Map constructor
		GameMap(WorldGen *w, Config* config);
		~GameMap();
		
		//Retrieves a chunk
		void get_chunk(
			ChunkID const&, 
			Block* buffer, 
			int stride_x = CHUNK_X, 
			int stride_xy = CHUNK_X * CHUNK_Y);

		//Retrieves a raw buffer associated with the given chunk
		void get_chunk_raw(
			ChunkID const&, 
			uint8_t* buffer,
			int* size);
		
		//Precaches a chunk
		void precache_chunk(ChunkID const&);
		
	private:
		//The world generator and config stuff
		WorldGen* world_gen;
		Config* config;
		
		//The game map
		tbb::concurrent_hash_map<ChunkID, std::pair<uint8_t*, int>, ChunkIDHashCompare> chunks;
	};
	
};


#endif

