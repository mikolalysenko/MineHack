#ifndef MAP_H
#define MAP_H

#include <pthread.h>

#include <map>
#include "chunk.h"
#include "worldgen.h"

namespace Game
{

	#define MAX_MAP_X	(1<<27)
	#define MAX_MAP_Y	(1<<27)
	#define MAX_MAP_Z	(1<<27)


	//This is basically a data structure which implements a caching/indexing system for chunks
	struct Map
	{
		//Map constructor
		Map(WorldGen *w) : world_gen(w)
		{
			pthread_rwlock_init(&map_lock, NULL);
		}
		
		//Retrieves the specific chunk
		Chunk* get_chunk(ChunkID const&);
		
		//Sets a block
		void set_block(int x, int y, int z, Block t);
		
		Block get_block(int x, int y, int z);
		
		//TODO: Implement fine grained locking for chunks
		pthread_rwlock_t map_lock;

	private:
		std::map<std::uint64_t, Chunk*>  chunks;
		WorldGen* world_gen;
	};
	
};


#endif

