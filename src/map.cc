#include "map.h"

using namespace std;


namespace Game
{

void Map::set_block(int x, int y, int z, Block b)
{
	ChunkId idx(x>>5, y>>5, z>>5);
	Chunk* c = get_chunk(idx);
	
	int bx = x&31,
		by = y&31,
		bz = z&31;
		
	c->data[bx+(by<<5)+(bz<<10)] = b;
	
}

Block Map::get_block(int x, int y, int z)
{
	ChunkId idx(x>>5, y>>5, z>>5);
	Chunk* c = get_chunk(idx);
	
	int bx = x&31,
		by = y&31,
		bz = z&31;
	
	return c->data[bx+(by<<5)+(bz<<10)];
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

};
