#ifndef PHYSICS_H
#define PHYSICS_H

#include <set>

#include <tbb/task.h>
#include <tbb/scalable_allocator.h>
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
		
		void set_block(Block b, int x, int y, int z);
		void mark_chunk(ChunkID const& chunk);	
		void update();
	
	private:

		struct BlockRecord
		{
			int x, y, z;
			Block b;
		};
	
		typedef tbb::concurrent_unordered_map<ChunkID, bool, ChunkIDHashCompare> chunk_set_t;
		typedef std::vector< BlockRecord, tbb::scalable_allocator<BlockRecord> > block_list_t;

		//Interface to separate sytems
		Config* config;
		GameMap* game_map;

		//The list of active chunks/block events
		tbb::spin_rw_mutex	chunk_set_lock;
		chunk_set_t active_chunks;
		block_list_t pending_blocks;
	
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
	
		void update_region(std::set<ChunkID> chunks, block_list_t blocks);
	};
};

#endif

