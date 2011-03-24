#include <vector>
#include <algorithm>

#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/scalable_allocator.h>

#include "constants.h"
#include "config.h"
#include "chunk.h"
#include "game_map.h"
#include "physics.h"

#define PHYSICS_DEBUG 1

#ifndef PHYSICS_DEBUG
#define DEBUG_PRINTF(...)
#else
#define DEBUG_PRINTF(...)  fprintf(stderr,__VA_ARGS__)
#endif


using namespace std;
using namespace tbb;

namespace Game
{

Physics::Physics(Config* cfg, GameMap* gmap) : config(cfg), game_map(gmap), base_tick(0)
{

}

Physics::~Physics()
{
}

void Physics::set_block(Block b, uint64_t t, int x, int y, int z)
{
	DEBUG_PRINTF("Writing block: %d, %d,%d,%d\n", b.int_val, x, y, z);

	ChunkID c(x/CHUNK_X, y/CHUNK_Y, z/CHUNK_Z);

	queuing_rw_mutex::scoped_lock L(chunk_set_lock, false);
	
	for(int dx=-1; dx<=1; ++dx)
	for(int dy=-1; dy<=1; ++dy)
	for(int dz=-1; dz<=1; ++dz)
	{
		ChunkID n(c.x+dx, c.y+dy, c.z+dz);
		active_chunks.insert(make_pair(n, true));
	}

	pending_blocks.push_back( (BlockRecord){t, x, y, z, b} );
}

//Marks a chunk for update
void Physics::mark_chunk(ChunkID const& c)
{
	queuing_rw_mutex::scoped_lock L(chunk_set_lock, false);
	
	for(int dx=-1; dx<=1; ++dx)
	for(int dy=-1; dy<=1; ++dy)
	for(int dz=-1; dz<=1; ++dz)
	{
		ChunkID n(c.x+dx, c.y+dy, c.z+dz);
		active_chunks.insert(make_pair(n, true));
		
		DEBUG_PRINTF("Marking chunk %d,%d,%d\n", n.x, n.y, n.z);
	}
}

//Updates a chunk
void Physics::update(uint64_t t)
{
	physics_tasks.wait();
	base_tick = t;
	struct TaskFunc
	{
		Physics* phys;
		void operator()() const { phys->update_main(); }
	};
	physics_tasks.run((TaskFunc){this});
}

//The main update loop
void Physics::update_main()
{
	if(base_tick == 0)
		return;
		
	DEBUG_PRINTF("Updating physics, base_tick = %ld\n", base_tick);

	chunk_set_t chunks;
	block_list_t blocks;
	{
		queuing_rw_mutex::scoped_lock L(chunk_set_lock, true);
		chunks.swap(active_chunks);
		blocks.swap(pending_blocks);
	}
	
	//An update task
	struct PhysicsUpdateTask
	{
		Physics*		physics;
		chunk_list_t	regions;
		block_list_t	region_blocks;
		
		void operator()()
		{
			//Sort regions for fast look up
			sort(regions.begin(), regions.end());
			
			//Sort blocks by time of write
			sort(region_blocks.begin(), region_blocks.end());
			
			physics->update_region(regions, region_blocks);
		}
	};
	
	//Partition the active chunk set into connected components
	task_group update_tasks;
	while(chunks.begin() != chunks.end())
	{
		DEBUG_PRINTF("Region: ");
		
		PhysicsUpdateTask task;
		task.physics = this;

		//Construct region via breadth first search
		task.regions.push_back( chunks.begin()->first );
		chunks.unsafe_erase(chunks.begin());
	
		int idx = 0;
			
		while(idx < task.regions.size())
		{
			auto c = task.regions[idx++];
		
			DEBUG_PRINTF("%d,%d,%d\n", c.x, c.y, c.z);
		
			for(int dx=-1; dx<=1; ++dx)
			for(int dy=-1; dy<=1; ++dy)
			for(int dz=-1; dz<=1; ++dz)
			{
				ChunkID n(c.x+dx, c.y+dy, c.z+dz);
			
				auto iter = chunks.find(n);
				if(iter == chunks.end())
					continue;
				
				chunks.unsafe_erase(iter);
				task.regions.push_back(n);
			}
		}
		
		//Add all blocks to this region
		for(int i=0; i<blocks.size(); ++i)
		{
			DEBUG_PRINTF("Processing pending write: %d,%d,%d; t=%ld\n", blocks[i].x, blocks[i].y, blocks[i].z, blocks[i].t);
		
			if(blocks[i].t >= base_tick + 16)
			{
				DEBUG_PRINTF("Got a packet out of order....\n");
				continue;
			}
		
			ChunkID c(
				blocks[i].x/CHUNK_X,
				blocks[i].y/CHUNK_Y,
				blocks[i].z/CHUNK_Z);
			
			//Check if chunk for this block write was removed.  if so, write must be in this region
			if(chunks.find(c) == chunks.end())
			{
				task.region_blocks.push_back(blocks[i]);
				
				//Remove block from the pending write queue
				blocks[i] = blocks[blocks.size()-1];
				blocks.pop_back();
				--i;
			}
		}
		
		//Run the task
		update_tasks.run(task);
	}
	
	//Put all the unhandled writes back in the work queue
	for(int i=0; i<blocks.size(); ++i)
	{
		DEBUG_PRINTF("Got a residual block: %d,%d,%d\n", blocks[i].x, blocks[i].y, blocks[i].z);
	
		set_block(blocks[i].b, blocks[i].t, blocks[i].x, blocks[i].y, blocks[i].z);
	}
	
	//Wait for all pending physics tasks to complete
	update_tasks.wait();
}


//Computes the next state of a block
Block Physics::update_block(
	Block center,
	Block left, 	//-x
	Block right,	//+x
	Block bottom,	//-y
	Block top, 		//+y
	Block front, 	//-z
	Block back)		//+z
{
	//FIXME: Physics implementation goes here
	
	if( center.type() == BlockType_Air &&
		top.type() == BlockType_Sand )
	{
		return Block( BlockType_Sand );
	}
	else if( center.type() == BlockType_Sand  &&
		bottom.type() == BlockType_Air )
	{
		return Block( BlockType_Air );
	}
	
	return center;
}



//Updates a chunk
bool Physics::update_chunk(
	Block* next,
	Block* current,
	int stride_x,
	int stride_xz)
{
	const int dx = 1;
	const int dy = stride_xz - (stride_x * (CHUNK_Z - 1) + CHUNK_X - 1);
	const int dz = stride_x - (CHUNK_X - 1);
	
	bool changed = false;

	int i = 0;
	while(true)
	{
		*next = update_block(
			current[ 0],
			current[-1],
			current[ 1],
			current[-stride_xz],
			current[ stride_xz],
			current[-stride_x],
			current[ stride_x]);
	
		if(*next != *current)
			changed = true;
	
		if(++i == CHUNK_SIZE)
		{
			break;
		}
		else if( i & (CHUNK_X-1) )
		{
			next	+= dx;
			current += dx;
		}
		else if( i & (CHUNK_X*CHUNK_Z-1) )
		{
			next	+= dz;
			current += dz;
		}
		else
		{
			next	+= dy;
			current += dy;
		}
	}
	
	return changed;
}

//Updates a list of chunks
void Physics::update_region(chunk_list_t const& marked_chunks, block_list_t const& blocks)
{
	uint32_t
		x_min = (1<<30), y_min = (1<<30), z_min = (1<<30),
		x_max = 0, y_max = 0, z_max = 0;
		
	//Construct the offset chunk list
	chunk_set_nl_t offset_chunk_set;
	
	DEBUG_PRINTF("Updating region: ");
	
	for(int i=0; i<marked_chunks.size(); ++i)
	{
		auto c = marked_chunks[i];			
	
		DEBUG_PRINTF("(%d,%d,%d), ", c.x, c.y, c.z);
	
		x_min = min(x_min, c.x - 1);
		x_max = max(x_max, c.x + 2);
		y_min = min(y_min, c.y - 1);
		y_max = max(y_max, c.y + 2);
		z_min = min(z_min, c.z - 1);
		z_max = max(z_max, c.z + 2);
		
		for(int dx=-1; dx<=1; ++dx)
		for(int dy=-1; dy<=1; ++dy)
		for(int dz=-1; dz<=1; ++dz)
		{
			offset_chunk_set.insert( ChunkID(
				c.x + dx,
				c.y + dy,
				c.z + dz) );
		}
	}
	DEBUG_PRINTF("\n");	
	
	//Unpack the update chunk list
	vector<ChunkID, scalable_allocator<ChunkID> > 
		chunks(offset_chunk_set.begin(), offset_chunk_set.end());
	
	DEBUG_PRINTF("chunks.size = %d, bounds = (%d-%d), (%d-%d), (%d-%d)\n", (int)chunks.size(),
		x_min, x_max, y_min, y_max, z_min, z_max);
	
	//Compute strides, need to pad by 1 in y dimension to avoid going oob
	int stride_x = (x_max - x_min) * CHUNK_X,
		stride_xz = stride_x * (z_max - z_min) * CHUNK_Z,
		size = stride_x * stride_xz * ((y_max - y_min) * CHUNK_Y + 2);
	
	//Allocate buffers
	auto front_buffer = (Block*)scalable_malloc(size * sizeof(Block));
	auto back_buffer = (Block*)scalable_malloc(size * sizeof(Block));
	auto update_times = (int8_t*)scalable_malloc(marked_chunks.size());
	memset(update_times, -1, marked_chunks.size());
	
	//Read chunks into memory
	parallel_for( blocked_range<int>(0, chunks.size(), 128),
		[&](blocked_range<int> rng)
	{
		for(auto i = rng.begin(); i != rng.end(); ++i)
		{
			auto c = chunks[i];
		
			int ox = c.x - x_min,
				oy = c.y - y_min,
				oz = c.z - z_min;
		
			int offset =  ox * CHUNK_X + 
						  oz * CHUNK_Z * stride_x + 
						 (oy * CHUNK_Y + 1) * stride_xz;


			DEBUG_PRINTF("Reading chunk: %d,%d,%d; %d,%d,%d; %d\n",
				c.x, c.y, c.z,
				ox, oy, oz,
				offset);
		
			game_map->get_chunk(c, front_buffer + offset, stride_x, stride_xz);
		}
	});
	
	//The current block index in the pending write queue
	int b_idx = 0;
	
	//Update the chunks (x16 to reduce overhead)
	for(int t=0; t<16; ++t)
	{
		DEBUG_PRINTF("Update, t = %d\n", t);
		
		//Compute physics for this region
		parallel_for( blocked_range<int>(0, chunks.size(), 32),
			[&]( blocked_range<int> rng )
		{
			for(auto i = rng.begin(); i != rng.end(); ++i)
			{
				auto c = chunks[i];
		
				int ox = c.x - x_min,
					oy = c.y - y_min,
					oz = c.z - z_min;
					
				int offset =  ox * CHUNK_X + 
							  oz * CHUNK_Z * stride_x + 
							 (oy * CHUNK_Y + 1) * stride_xz;
				
				if(update_chunk(
					back_buffer  + offset,
					front_buffer + offset,
					stride_x,
					stride_xz))
				{
					//Check if the chunk was in the marked set
					auto pos = lower_bound(marked_chunks.begin(), marked_chunks.end(), c);
					if(pos != marked_chunks.end() && *pos == c)
					{
						int idx = pos - marked_chunks.begin();
						update_times[idx] = t;
					
						DEBUG_PRINTF("Updated chunk: %d at t=%d", idx, t);
					
					}
				}
			}
		});
		
		//Apply pending writes (must be done sequentially)
		while(b_idx < blocks.size() &&
			blocks[b_idx].t <= t + base_tick)
		{
			int x = blocks[b_idx].x,
				y = blocks[b_idx].y,
				z = blocks[b_idx].z;
				
			auto b = blocks[b_idx].b;
		
			int cx = x / CHUNK_X,
				cy = y / CHUNK_Y,
				cz = z / CHUNK_Z;
						
			int ox = cx - x_min,
				oy = cy - y_min,
				oz = cz - z_min;
		
			int offset = (ox * CHUNK_X + (x % CHUNK_X)) +
						 (oz * CHUNK_Z + (z % CHUNK_Z)) * stride_x + 
						 (oy * CHUNK_Y + (y % CHUNK_Y) + 1) * stride_xz;
		
			DEBUG_PRINTF("Writing block: %d,%d,%d, b = %d, c=%d,%d,%d\n, o=%d,%d,%d, offs=%d\n", x, y, z, b.int_val, cx, cy, cz, ox, oy, oz, offset);
			back_buffer[offset] = b;
			
			//Check if the chunk was in the marked set and update modify time stamp
			ChunkID c(cx, cy, cz);
			auto pos = lower_bound(marked_chunks.begin(), marked_chunks.end(), c);
			if(pos != marked_chunks.end() && *pos == c)
			{
				int idx = pos - marked_chunks.begin();
				update_times[idx] = t;
			
				DEBUG_PRINTF("Updated chunk: %d at t=%d", idx, t);
			}
			
			//Increment pointer
			++b_idx;
		}
		
		auto tmp = back_buffer;
		back_buffer = front_buffer;
		front_buffer = tmp;
	}
	
	//Check for chunks which changed, and update them in the map
	parallel_for( blocked_range<int>(0, marked_chunks.size(), 128),
		[&]( blocked_range<int> rng )
	{
		for(auto i = rng.begin(); i != rng.end(); ++i)
		{
			if(update_times[i] < 0)
				continue;
				
			uint64_t ticks = base_tick + update_times[i];
		
			auto c = marked_chunks[i];
	
			int ox = c.x - x_min,
				oy = c.y - y_min,
				oz = c.z - z_min;
			
			int offset =  ox * CHUNK_X + 
						  oz * CHUNK_Z * stride_x + 
						 (oy * CHUNK_Y + 1) * stride_xz;
		
			DEBUG_PRINTF("Writing chunk: %d,%d,%d; %d,%d,%d; %d, t=%d\n",
				c.x, c.y, c.z,
				ox, oy, oz,
				offset, ticks);
			
			game_map->update_chunk(
				c, ticks,
				front_buffer + offset,
				stride_x,
				stride_xz);

			if(update_times[i] == 15)
			{
				DEBUG_PRINTF("Writing chunk: %d,%d,%d\n", c.x, c.y, c.z);
				mark_chunk(c);
			}
		}
	});
	
	scalable_free(update_times);
	scalable_free(front_buffer);
	scalable_free(back_buffer);
}

};
