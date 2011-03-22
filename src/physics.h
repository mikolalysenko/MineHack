#ifndef PHYSICS_H
#define PHYSICS_H

#include <set>

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
		~Physics();
		
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
	
		//Computes the next state of a single block
		static Block update_block(
			Block center,
			Block left, Block right,
			Block bottom, Block top,
			Block front, Block back);

		//Updates a single chunk	
		static void update_chunk(
			Block* next,
			Block* prev,
			int stride_x,
			int stride_xz);
	
		void update_region(std::set<ChunkID> chunks);
	};
};

#endif

