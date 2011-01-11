#ifndef MAP_H
#define MAP_H

#include <pthread.h>

#include <Eigen/Core>

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
	
	//A region is a subset of a map which is used for locking purposes
	struct Region
	{
		Eigen::Vector3i lo, hi;
		
		Region() : lo(0, 0, 0), hi(MAX_MAP_X, MAX_MAP_Y, MAX_MAP_Z)
		{ }
		
		Region(Eigen::Vector3i l, Eigen::Vector3i h) : lo(l), hi(h) {}
		
		Region(ChunkID const& id) :
			lo(id.x*32, id.y*32, id.z*32),
			hi((id.x+1)*32, (id.y+1)*32, (id.z+1)*32)
		{ }
		
		Region(int x, int y, int z) :
			lo(x, y, z),
			hi(x+1, y+1, z+1)
		{ }
		
		Region(Region const& other) :
			lo(other.lo),
			hi(other.hi)
		{ }
	};
	
	//Acquires a read lock for a particular region
	struct RegionReadLock 
	{
		Map* map;
		Region region;
		
		RegionReadLock(Map* m, Region const& r);
		~RegionReadLock();
	};
	
	//Acquires a write lock for a particular region
	struct RegionWriteLock
	{
		Map* map;
		Region region;
	
		RegionWriteLock(Map* m, Region const& r);
		~RegionWriteLock();
	};
};


#endif

