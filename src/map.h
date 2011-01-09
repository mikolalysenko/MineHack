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
		
	private:
		std::map<std::uint64_t, Chunk*>  chunks;
		WorldGen* world_gen;
	};
};


#endif

