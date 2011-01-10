#include "map.h"

using namespace std;


namespace Game
{

void Map::set_block(int x, int y, int z, Block b)
{
	ChunkId idx(x>>5, y>>5, z>>5);
	Chunk* c = get_chunk(idx);
	c->data[x&31][y&31][z&31] = b;
}

Block Map::get_block(int x, int y, int z)
{
	ChunkId idx(x>>5, y>>5, z>>5);
	Chunk* c = get_chunk(idx);
	return c->data[x&31][y&31][z&31];
}

//Retrieves a particular chunk from the map
Chunk* Map::get_chunk(const ChunkId& idx)
{
	uint64_t hash = idx.hash();
	
	//Check if contained in map
	auto iter = chunks.find(hash);
	
	if(iter == chunks.end())
	{
		auto nchunk = world_gen->generate_chunk(idx);
		chunks[hash] = nchunk;
		return nchunk;
	}
	
	return (*iter).second;
}

//Acquires a read lock for a particular region
RegionReadLock::RegionReadLock(Map* map, const Region& r)
{
}

RegionReadLock::~RegionReadLock()
{
}
	
//Acquires a write lock for a particular region
RegionWriteLock::RegionWriteLock(Map* map, const Region& r)
{
}

RegionWriteLock::~RegionWriteLock()
{
};

};
