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
		delete iter->second;
	}
	delete world_gen;
}

//Retrieves a mutable accessor to the chunk buffer
void GameMap::get_chunk_buffer(ChunkID const& chunk_id, accessor& acc)
{
	if(!chunks.find(acc, chunk_id))
	{
		if(chunks.insert(acc, chunk_id))
		{
			Block buffer[CHUNK_SIZE];
			world_gen->generate_chunk(chunk_id, buffer, CHUNK_X, CHUNK_X*CHUNK_Y);
			
			acc->second = new ChunkBuffer();
			acc->second->compress_chunk(buffer, CHUNK_X, CHUNK_X*CHUNK_Y);
		}
		else
		{
			chunks.find(acc, chunk_id);
		}
	}
}

//Retrieves a const accessor to the chunk buffer, slightly different since we
//can't reuse the accessor for the 
void GameMap::get_chunk_buffer(ChunkID const& chunk_id, const_accessor& acc)
{
	if(!chunks.find(acc, chunk_id))
	{
		accessor  mut_acc;
		if(chunks.insert(mut_acc, chunk_id))
		{
			Block buffer[CHUNK_SIZE];
			world_gen->generate_chunk(chunk_id, buffer, CHUNK_X, CHUNK_X*CHUNK_Y);
			
			mut_acc->second = new ChunkBuffer();
			mut_acc->second->compress_chunk(buffer, CHUNK_X, CHUNK_X*CHUNK_Y);
			mut_acc.release();
		}
		
		chunks.find(acc, chunk_id);
	}
}

//Chunk copying methods
void GameMap::get_chunk(ChunkID const& chunk_id, Block* buffer, int stride_x,  int stride_xz)
{
	const_accessor acc;
	get_chunk_buffer(chunk_id, acc);
	acc->second->decompress_chunk(buffer, stride_x, stride_xz);
}

//Protocol buffer methods
Network::ServerPacket* GameMap::get_net_chunk(ChunkID const& chunk_id, uint64_t timestamp)
{
	//Use const accessor first to avoid exclusive locking
	const_accessor acc;
	get_chunk_buffer(chunk_id, acc);
	if(acc->second->last_modified() <= timestamp)
		return NULL;

	auto packet = new Network::ServerPacket();
	auto chunk = packet->mutable_chunk_response();
	
	if(!acc->second->serialize_to_protocol_buffer(*chunk))
	{
		//If this fails because the buffer is not ready, we resort to exclusive
		//locking in order to generate the cache
		acc.release();
		
		accessor mut_acc;
		get_chunk_buffer(chunk_id, mut_acc);
		mut_acc->second->cache_protocol_buffer_data();
		mut_acc->second->serialize_to_protocol_buffer(*chunk);
	}
	else
	{	
		acc.release();
	}
	
	chunk->set_x(chunk_id.x);
	chunk->set_y(chunk_id.y);
	chunk->set_z(chunk_id.z);
	
	return packet;
}

};
