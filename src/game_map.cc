#include <cstdlib>
#include <stdint.h>

#include <tbb/concurrent_hash_map.h>

#include "constants.h"
#include "config.h"
#include "chunk.h"
#include "game_map.h"

using namespace tbb;
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
		//Need to generate chunk
		if(chunks.insert(acc, chunk_id))
		{
			world_gen->generate_chunk(chunk_id, data, stride_x, stride_xy);
			acc->second = compress_chunk(data, stride_x, stride_xy);
			acc->second.last_modified = 1;
			return;
		}
		
		//Weird situation, thread got preempted before we could insert new key
		chunks.find(acc, chunk_id);
	}
	
	//Decompress the chunk
	decompress_chunk(acc->second, data, stride_x, stride_xy);
}

//Serializes a network chunk packet if the the timestamp < last modified timestamp
Network::ServerPacket* GameMap::get_net_chunk(ChunkID const& chunk_id, uint64_t timestamp)
{
	chunk_map_t::accessor acc;
	
	if(!chunks.find(acc, chunk_id))
	{
		if(chunks.insert(acc, chunk_id))
		{
			//Allocate a temporary buffer
			Block buffer[CHUNK_SIZE];
			world_gen->generate_chunk(chunk_id, buffer, CHUNK_X, CHUNK_X*CHUNK_Y);
			
			//Store result
			acc->second = compress_chunk(buffer, CHUNK_X, CHUNK_X*CHUNK_Y);
			acc->second.last_modified = 1;
		}
		else
		{
			chunks.find(acc, chunk_id);
		}
	}
	
	if(acc->second.last_modified <= timestamp)
		return NULL;
	
	auto packet = new Network::ServerPacket();
	auto chunk = packet->mutable_chunk_response();
	
	chunk->set_x(chunk_id.x);
	chunk->set_y(chunk_id.y);
	chunk->set_z(chunk_id.z);
	chunk->set_last_modified(acc->second.last_modified);
	chunk->set_data(acc->second.data, acc->second.size);
	
	return packet;
}

};
