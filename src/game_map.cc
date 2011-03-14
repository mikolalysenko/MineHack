#include <map>
#include <string>

#include <cstring>
#include <cstdlib>
#include <cstdint>

#include "constants.h"
#include "config.h"
#include "chunk.h"
#include "map.h"

using namespace std;


namespace Game
{

GameMap::GameMap(Config* cfg) : world_gen(new WorldGen(cfg)), config(cfg)
{
}

GameMap::~GameMap()
{
	//Iterate over all chunk buffers and deallocate
	for(auto iter = chunks.begin(); iter != chunks.end(); ++iter)
	{
		free(iter->second.data);
	}

	delete world_gen;
}


//Retrieves a chunk
void GameMap::get_chunk(
	ChunkID const& chunk_id, 
	Block* data, 
	int stride_x, 
	int stride_xy)
{
	
	chunk_map_t::accessor acc;
	
	if(!chunks.find(acc, chunk_id))
	{
		//Generate chunk
		if(chunks.insert(acc, chunk_id))
		{
			world_gen->generate_chunk(chunk_id, data, stride_x, stride_xy);
			acc->second = compress_chunk(data, stride_x, stride_xy);
			return;
		}
		
		//Weird situation, thread got preempted before we could insert new key
		chunks.find(acc, chunk_id);
	}
	
	//Decompress the chunk
	decompress_chunk(acc->second, data, stride_x, stride_xy);
}

};
