#include "map.h"

using namespace std;


namespace Game
{

void Map::set_block(int x, int y, int z, Block b)
{
	ChunkID idx(x>>5, y>>5, z>>5);
	Chunk* c = get_chunk(idx);
	c->set(x&31, y&31, z&31, b);
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
	
	if(iter == chunks.end())
	{
		cout << "chunk not found, generating chunk" << endl;
		auto nchunk = world_gen->generate_chunk(idx);
		chunks[hash] = nchunk;
		return nchunk;
	}
	
	auto res = (*iter).second;
	return (*iter).second;
}

};
