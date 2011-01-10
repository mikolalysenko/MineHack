#ifndef MAP_H
#define MAP_H

#include <map>
#include "chunk.h"
#include "worldgen.h"

namespace Game
{

	//This is basically a data structure which implements a caching/indexing system for chunks
	struct Map
	{
		//Map constructor
		Map(WorldGen *w) : world_gen(w) {}
		
		//Retrieves the specific chunk
		Chunk* get_chunk(const ChunkId&);
		
		//Sets a block
		void set_block(int x, int y, int z, Block t);
		
		Block get_block(int x, int y, int z);
		
	private:
		std::map<std::uint64_t, Chunk*>  chunks;
		WorldGen* world_gen;
	};
};


#endif

