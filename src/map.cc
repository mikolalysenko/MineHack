#include "map.h"

using namespace std;


namespace Game
{

void Map::set_block(int x, int y, int z, Block b)
{
	ChunkID idx(x>>5, y>>5, z>>5);
	Chunk* c = get_chunk(idx);
	c->set(bx&31, by&31, bz&31, b);
}

Block Map::get_block(int x, int y, int z)
{
	ChunkID idx(x>>5, y>>5, z>>5);
	Chunk* c = get_chunk(idx);
	return c->get(x&31, y&31, z&31);
}

//Retrieves a particular chunk from the map
Chunk* Map::get_chunk(ChunkID const& idx)
{
	uint64_t hash = idx.hash();
	
	//Check if contained in map
	auto iter = chunks.find(hash);
	
	cout << "lookup chunk: " << idx.x << ',' << idx.y << ',' << idx.z << endl;
	
	if(iter == chunks.end())
	{
		cout << "not found, generating chunk" << endl;
		auto nchunk = world_gen->generate_chunk(idx);
		chunks[hash] = nchunk;
		return nchunk;
	}
	
	auto res = (*iter).second;
	
	cout << "got chunk: " << res->idx.x << ',' << res->idx.y << ',' << res->idx.z << endl;
	
	return (*iter).second;
}

//Acquires a read lock for a particular region
RegionReadLock::RegionReadLock(Map* m, const Region& r) : map(m), region(r)
{
	pthread_rwlock_rdlock(&(map->map_lock));
}

RegionReadLock::~RegionReadLock()
{
	pthread_rwlock_unlock(&(map->map_lock));
}
	
//Acquires a write lock for a particular region
RegionWriteLock::RegionWriteLock(Map* m, const Region& r) : map(m), region(r)
{
	pthread_rwlock_wrlock(&(map->map_lock));	
}

RegionWriteLock::~RegionWriteLock()
{
	pthread_rwlock_unlock(&(map->map_lock));
}

};
