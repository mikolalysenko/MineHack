#include "map.h"

using namespace std;


namespace Game
{

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
