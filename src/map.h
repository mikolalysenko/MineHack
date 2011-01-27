#ifndef MAP_H
#define MAP_H

#include <string>
#include <cstdint>
#include <cstdlib>

#include <pthread.h>

#include <tcutil.h>
#include <tchdb.h>

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
		Map(WorldGen *w, std::string const& filename);
		~Map();
		
		//Retrieves the specific chunk
		void get_chunk(ChunkID const&, Chunk* res);
		
		//Sets a block
		void set_block(int x, int y, int z, Block t);
		
		//Gets a block from a chunk
		Block get_block(int x, int y, int z);
		
		//Updates light values
		void update_lighting(ChunkID const&, LightVal const*);
		
	private:
		TCHDB* map_db;
		
		//When held, the world generator is executing.
		//Since it is not assumed to be reentrant, we can only do one thing at a time with the world generator
		pthread_mutex_t world_gen_lock;
		WorldGen* world_gen;
	};
	
};


#endif

