#ifndef PHYSICS_H
#define PHYSICS_H

#include <set>

#include <tbb/task.h>
#include <tbb/task_group.h>
#include <tbb/scalable_allocator.h>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/queuing_rw_mutex.h>

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
		
		void set_block(Block b, uint64_t t, int x, int y, int z);
		void mark_chunk(ChunkID const& chunk);	
		void update(uint64_t t);
	
	private:

		struct BlockRecord
		{
			uint64_t t;
			int x, y, z;
			Block b;
			
			bool operator==(BlockRecord const& other) const
			{
				return t==other.t && x == other.x && y == other.y && z == other.z && b == other.b;
			}
			
			bool operator<(BlockRecord const& other) const
			{
				return t < other.t;
			}
		};
	
		typedef tbb::concurrent_unordered_map<ChunkID, bool, ChunkIDHashCompare> chunk_set_t;
		typedef std::set<ChunkID, std::less<ChunkID>, tbb::scalable_allocator<ChunkID> >  chunk_set_nl_t;
		typedef std::vector< BlockRecord, tbb::scalable_allocator<BlockRecord> > block_list_t;
		typedef std::vector< ChunkID, tbb::scalable_allocator<ChunkID> > chunk_list_t;

		//Interface to separate sytems
		Config* config;
		GameMap* game_map;
		
		//The physics task group
		uint64_t base_tick;
		tbb::task_group	physics_tasks;

		//The list of active chunks/block events
		tbb::queuing_rw_mutex	chunk_set_lock;
		chunk_set_t active_chunks;
		block_list_t pending_blocks;
	
		//Computes the next state of a single block
		static Block update_block(
			Block center,
			Block left, Block right,
			Block bottom, Block top,
			Block front, Block back);

		//Updates a single chunk	
		static bool update_chunk(
			Block* next,
			Block* prev,
			int stride_x,
			int stride_xz);
	
		//Updates a region
		void update_region(chunk_list_t const& chunks, block_list_t const& blocks);
		
		//Start of the update loop
		void update_main();
	};
};

#endif

