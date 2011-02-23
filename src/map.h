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


	struct VisBuffer
	{
		void*	ptr;
		int		size;
	};
	
	//Calculates a key for a visibility buffer
	uint64_t pos_to_vis_key(double x, double y, double z);
	uint64_t chunkid_to_vis_key(ChunkID const&);

	//This is basically a data structure which implements a caching/indexing system for chunks
	struct Map
	{
		//Map constructor
		Map(WorldGen *w, std::string const& filename);
		~Map();
		
		//Retrieves the specific chunk
		void get_chunk(ChunkID const&, Chunk* res);
		
		//Returns a chunk with only visible cells labeled
		bool get_surface_chunk(ChunkID const&, Chunk* res);
		
		//Sets a block
		void set_block(int x, int y, int z, Block t);
		
		//Gets a block from a chunk
		Block get_block(int x, int y, int z);
		
		//Sends a visibility buffer over a socket
		bool send_vis_buffer(uint64_t key, int socket);
		
		//Generates a visibility buffer
		void gen_vis_buffer(uint64_t key, bool no_lock = false);
		
	private:
		TCHDB* map_db;
		
		//A set of precalculated visibility buffers (these are regenerated constantly)
		pthread_mutex_t	vis_lock;
		std::map< uint64_t, VisBuffer >	vis_buffers;
		
		//Invalidates a visibility buffer
		void invalidate_vis_buffer(uint64_t);
		
		WorldGen* world_gen;
	};
	
};


#endif

