#ifndef MAP_H
#define MAP_H

#include <Eigen/Core>

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
		
		//Sets a block
		void set_block(int x, int y, int z, Block t);
		
		Block get_block(int x, int y, int z);
		
	private:
		std::map<std::uint64_t, Chunk*>  chunks;
		WorldGen* world_gen;
	};
	
	//A region is a subset of a map which is used for locking purposes
	struct Region
	{
		Eigen::Vector3i lo, hi;
		
		Region() : lo(0, 0, 0), hi((1<<21), (1<<21), (1<<21))
		{ }
		
		Region(const ChunkId& id) :
			lo(id.x*32, id.y*32, id.z*32),
			hi((id.x+1)*32, (id.y+1)*32, (id.z+1)*32)
		{ }
		
		Region(int x, int y, int z) :
			lo(x, y, z),
			hi(x+1, y+1, z+1)
		{ }
	};
	
	//Acquires a read lock for a particular region
	struct RegionReadLock
	{
		RegionReadLock(Map* map, const Region& r);
		~RegionReadLock();
	};
	
	//Acquires a write lock for a particular region
	struct RegionWriteLock
	{
		RegionWriteLock(Map* map, const Region& r);
		~RegionWriteLock();
	};
};


#endif

