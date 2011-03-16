#ifndef GAME_MAP_H
#define GAME_MAP_H

#include <stdint.h>

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
		//Types
		typedef tbb::concurrent_hash_map<ChunkID, ChunkBuffer, ChunkIDHashCompare> chunk_map_t;
		typedef chunk_map_t::accessor accessor;
		typedef chunk_map_t::const_accessor const_accessor;

		//Map constructor
		GameMap(Config* config);
		~GameMap();
		
		//Retrieves a chunk buffer for writing
		void get_chunk_buffer(
			ChunkID const&,
			accessor);
			
		//Retrieves a chunk buffer for reading
		void get_chunk_buffer(
			ChunkID const&,
			const_accessor);
		
		//Retrieves a chunk
		void get_chunk(
			ChunkID const&, 
			Block* buffer, 
			int stride_x = CHUNK_X, 
			int stride_xy = CHUNK_X * CHUNK_Y);

		//Retrieves a raw buffer associated with the given chunk
		Network::ServerPacket* get_net_chunk(ChunkID const&, uint64_t timestamp);
					
	private:
		//The world generator and config stuff
		WorldGen* world_gen;
		Config* config;
		
		//The game map
		chunk_map_t chunks;
	};
	
};


#endif

