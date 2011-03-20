#ifndef PHYSICS_H
#define PHYSICS_H

#include <tbb/task.h>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/spin_rw_mutex.h>

#include "constants.h"
#include "config.h"
#include "chunk.h"
#include "game_map.h"

namespace Game
{
	struct Physics
	{
		Physics(Config* config, GameMap* game_map);


		void mark_chunk(ChunkID const& chunk);	
		void update();
	
	private:

		//Interface to separate sytems
		Config* config;
		GameMap* game_map;
	
		typedef tbb::concurrent_unordered_map<ChunkID, bool, ChunkIDHashCompare> chunk_set_t;

		//The list of active chunks	
		tbb::spin_rw_mutex	chunk_set_lock;
		chunk_set_t active_chunks;
	
		void update_region(std::vector<ChunkID> chunks);
	};
};

#endif

