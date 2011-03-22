#include <vector>

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

Physics::Physics(Config* cfg, GameMap* gmap) : config(cfg), game_map(gmap)
{
}

Physics::~Physics()
{
}

void Physics::set_block(Block b, int x, int y, int z)
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

	pending_blocks.push_back( (BlockRecord){x, y, z, b} );
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
	chunk_set_t chunks;
	block_list_t blocks;
	{
		queuing_rw_mutex::scoped_lock L(chunk_set_lock, true);
		chunks.swap(active_chunks);
		blocks.swap(pending_blocks);
	}

	//Partition the active chunk set into connected components
	vector< chunk_set_nl_t > regions;
	while(chunks.begin() != chunks.end())
	{
		DEBUG_PRINTF("Region: ");

	
		//Mark first chunk on the tree
		vector< ChunkID, scalable_allocator<ChunkID> > to_visit;
		to_visit.push_back( chunks.begin()->first );
		chunks.unsafe_erase(chunks.begin());
		
		int idx = 0;
				
		while(idx < to_visit.size())
		{
			auto c = to_visit[idx++];
			
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
				to_visit.push_back(n);
			}
		}
		
		regions.push_back( chunk_set_nl_t(to_visit.begin(), to_visit.end()) );
	}
	
	//Update all regions in parallel
	parallel_for( blocked_range<int>(0, regions.size(), 1), 
		[&]( blocked_range<int> rng )
	{
		for(auto i = rng.begin(); i != rng.end(); ++i)
		{
			update_region( t, regions[i], blocks );
		}
	});
	
	
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
		top.type() == BlockType_Water )
	{
		return Block( BlockType_Water );
	}
	
	return center;
}



//Updates a chunk
void Physics::update_chunk(
	Block* next,
	Block* current,
	int stride_x,
	int stride_xz)
{
	const int dx = 1;
	const int dy = stride_xz - (stride_x * (CHUNK_Z - 1) + CHUNK_X - 1);
	const int dz = stride_x - (CHUNK_X - 1);

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
}

//Updates a list of chunks
void Physics::update_region(uint64_t ticks, chunk_set_nl_t const& marked_chunk_set, block_list_t const& blocks)
{
	uint32_t
		x_min = (1<<30), y_min = (1<<30), z_min = (1<<30),
		x_max = 0, y_max = 0, z_max = 0;
		
	
	//Construct the offset chunk list
	chunk_set_nl_t offset_chunk_set;
	
	DEBUG_PRINTF("Updating region: ");
	
	for(auto iter = marked_chunk_set.begin(); iter != marked_chunk_set.end(); ++iter)
	{
		DEBUG_PRINTF("(%d,%d,%d), ", iter->x, iter->y, iter->z);
	
		x_min = min(x_min, iter->x - 1);
		x_max = max(x_max, iter->x + 2);
		y_min = min(y_min, iter->y - 1);
		y_max = max(y_max, iter->y + 2);
		z_min = min(z_min, iter->z - 1);
		z_max = max(z_max, iter->z + 2);
		
		for(int dx=-1; dx<=1; ++dx)
		for(int dy=-1; dy<=1; ++dy)
		for(int dz=-1; dz<=1; ++dz)
		{
			offset_chunk_set.insert( ChunkID(
				iter->x + dx,
				iter->y + dy,
				iter->z + dz) );
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
	
	//Read chunks into memory
	parallel_for( blocked_range<int>(0, chunks.size()),
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
	
	//Apply pending writes
	for(auto iter = blocks.begin(); iter != blocks.end(); ++iter)
	{
		int cx = iter->x / CHUNK_X,
			cy = iter->y / CHUNK_Y,
			cz = iter->z / CHUNK_Z;
			
		if( marked_chunk_set.find(ChunkID(cx,cy,cz)) == marked_chunk_set.end() )
			continue;
						
		int ox = cx - x_min,
			oy = cy - y_min,
			oz = cz - z_min;
		
		int offset = (ox * CHUNK_X + (iter->x % CHUNK_X)) +
					 (oz * CHUNK_Z + (iter->z % CHUNK_Z)) * stride_x + 
					 (oy * CHUNK_Y + (iter->y % CHUNK_Y) + 1) * stride_xz;
		
		DEBUG_PRINTF("Writing block: %d,%d,%d, b = %d, c=%d,%d,%d\n, o=%d,%d,%d, offs=%d\n", iter->x, iter->y, iter->z, iter->b.int_val, cx, cy, cz, ox, oy, oz, offset);
		
		front_buffer[offset] = iter->b;
	}
	
	/*
	//Update the chunks (x16 to reduce overhead)
	for(int t=0; t<16; ++t)
	{
		DEBUG_PRINTF("Update, t = %d\n", t);
		parallel_for( blocked_range<int>(0, chunks.size(), 16),
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
				
				update_chunk(
					back_buffer  + offset,
					front_buffer + offset,
					stride_x,
					stride_xz);
			}
		});
		
		auto tmp = back_buffer;
		back_buffer = front_buffer;
		front_buffer = tmp;
	}
	*/
	
	//Check for chunks which changed, and update them in the map
	for(auto iter=marked_chunk_set.begin(); iter!=marked_chunk_set.end(); ++iter)
	{
		auto c = *iter;
	
		int ox = c.x - x_min,
			oy = c.y - y_min,
			oz = c.z - z_min;
			
		int offset =  ox * CHUNK_X + 
					  oz * CHUNK_Z * stride_x + 
					 (oy * CHUNK_Y + 1) * stride_xz;
		
		DEBUG_PRINTF("Writing chunk: %d,%d,%d; %d,%d,%d; %d\n",
			c.x, c.y, c.z,
			ox, oy, oz,
			offset);
	
		
		if(game_map->update_chunk(
			c, ticks,
			front_buffer + offset,
			stride_x,
			stride_xz))
		{
			mark_chunk(c);
		}
	}
	
	scalable_free(front_buffer);
	scalable_free(back_buffer);
}

};
