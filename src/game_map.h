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
		typedef tbb::concurrent_hash_map<ChunkID, ChunkBuffer*, ChunkIDHashCompare> chunk_map_t;
		typedef chunk_map_t::accessor accessor;
		typedef chunk_map_t::const_accessor const_accessor;

		//Map constructor
		GameMap(Config* config);
		~GameMap();
		
		//Accessor methods
		void get_chunk_buffer(accessor&, ChunkID const&);
		void get_chunk_buffer(const_accessor&, ChunkID const&);
		void get_surface_chunk_buffer(accessor&, ChunkID const&);
		void get_surface_chunk_buffer(const_accessor&, ChunkID const&);
		
		//Block accessor methods
		Block get_block(int x, int y, int z);
		bool set_block(Block b, int x, int y, int z, uint64_t t);
		
		//Chunk copying methods
		void get_chunk(
			ChunkID const&, 
			Block* buffer, 
			int stride_x = CHUNK_X, 
			int stride_xz = CHUNK_X * CHUNK_Z);

		//Protocol buffer methods
		Network::ServerPacket* get_net_chunk(ChunkID const&, uint64_t timestamp);
					
	private:
		//The world generator and config stuff
		WorldGen* world_gen;
		Config* config;
		
		//The game map
		//When operating on surface chunks and chunks, must lock surface chunks first.
		chunk_map_t chunks, surface_chunks;
		
		//Generates a surface chunk
		void generate_surface_chunk(accessor&, ChunkID const&);
	};
	
};


#endif

