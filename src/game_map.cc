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
	
	for(auto iter = surface_chunks.begin(); iter != surface_chunks.end(); ++iter)
	{
		delete iter->second;
	}
	
	delete world_gen;
}

//Retrieves a mutable accessor to the chunk buffer
void GameMap::get_chunk_buffer(accessor& acc, ChunkID const& chunk_id)
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
void GameMap::get_chunk_buffer(const_accessor& acc, ChunkID const& chunk_id)
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

//Retrieves a surface chunk buffer
void GameMap::get_surface_chunk_buffer(accessor& acc, ChunkID const& chunk_id)
{
	if(!surface_chunks.find(acc, chunk_id))
	{
		if(surface_chunks.insert(acc, chunk_id))
		{
			acc->second = new ChunkBuffer();
			generate_surface_chunk(acc, chunk_id);
			return;
		}
		
		surface_chunks.find(acc, chunk_id);
	}
}

//Retrieves a surface chunk buffer (const version
void GameMap::get_surface_chunk_buffer(const_accessor& acc, ChunkID const& chunk_id)
{
	if(!surface_chunks.find(acc, chunk_id))
	{
		accessor mut_acc;
		if(surface_chunks.insert(mut_acc, chunk_id))
		{
			mut_acc->second = new ChunkBuffer();
			generate_surface_chunk(mut_acc, chunk_id);
			mut_acc.release();
		}
		
		surface_chunks.find(acc, chunk_id);
	}
}

//Queries a single block within the map
Block GameMap::get_block(int x, int y, int z)
{
	const_accessor acc;
	get_chunk_buffer(acc, ChunkID(x/CHUNK_X, y/CHUNK_Y, z/CHUNK_Z));
	return acc->second->get_block(x%CHUNK_X, y%CHUNK_Y, z%CHUNK_Z);
}

//Sets a block in the map
bool GameMap::set_block(Block b, int x, int y, int z, uint64_t t)
{
	//FIXME: Lock surface chunks only if t is a transparent block

	//Update chunk
	accessor acc;
	get_chunk_buffer(acc, ChunkID(x/CHUNK_X, y/CHUNK_Y, z/CHUNK_Z));
	if(acc->second->set_block(b, x%CHUNK_X, y%CHUNK_Y, z%CHUNK_Z, t))
	{
		return true;
	}
	
	//FIXME: Update surface chunks here
	
	return false;
}

//Chunk copying methods
void GameMap::get_chunk(ChunkID const& chunk_id, Block* buffer, int stride_x,  int stride_xz)
{
	const_accessor acc;
	get_chunk_buffer(acc, chunk_id);
	acc->second->decompress_chunk(buffer, stride_x, stride_xz);
}

//Generates a surface chunk and stores it in the cache
void GameMap::generate_surface_chunk(accessor& acc, ChunkID const& chunk_id)
{
	const int stride_x = 3 * CHUNK_X;
	const int stride_xz = 3 * stride_x * CHUNK_Z;
		
	//Lock the neighboring chunks and cache them
	const_accessor chunks[3][3][3];
	Block buffer[CHUNK_SIZE*3*3*3];
	uint64_t timestamp = 1;	

	//Lock the surrounding buffers for reading, always use order y-z-x
	for(int j=0; j<3; ++j)
	for(int k=0; k<3; ++k)
	for(int i=0; i<3; ++i)
	{
		get_chunk_buffer(chunks[i][j][k],
						ChunkID(chunk_id.x-1+i,
								chunk_id.y-1+j,
								chunk_id.z-1+k));

		int ix = i * CHUNK_X,
			iy = j * CHUNK_Y,
			iz = k * CHUNK_Z;
			
		Block* ptr = buffer + ix + iz * stride_x + iy * stride_xz;
		chunks[i][j][k]->second->decompress_chunk(ptr, stride_x, stride_xz);
		timestamp = max(timestamp, chunks[i][j][k]->second->last_modified());
	}
	
	//Traverse to find surface chunks
	int i  = CHUNK_X + CHUNK_Z * stride_x + CHUNK_Y * stride_xz,
		dy = stride_xz - CHUNK_Z*stride_x,
		dz = stride_x - CHUNK_X,
		dx = 1;
		
	bool empty = true;
	
	for(int y=0; y<CHUNK_Y; ++y, i+=dy)
	for(int z=0; z<CHUNK_Z; ++z, i+=dz)
	for(int x=0; x<CHUNK_X; ++x, i+=dx)
	{
		if( buffer[i].transparent() ||
			buffer[i-1].transparent() ||
			buffer[i+1].transparent() ||
			buffer[i-stride_x].transparent() ||
			buffer[i+stride_x].transparent() ||
			buffer[i-stride_xz].transparent() ||
			buffer[i+stride_xz].transparent() )
		{
			if(buffer[i].type() != BlockType_Air)
				empty = false;
			continue;
		}
		
		buffer[i] = BlockType_Stone;
	}
	
	//Construct the chunk buffer and save it
	acc->second->compress_chunk(buffer, stride_x, stride_xz);
	acc->second->set_last_modified(timestamp);
	acc->second->set_empty_surface(empty);
	acc->second->cache_protocol_buffer_data();		//Eagerly cache protocol buffer
}


//Protocol buffer methods
Network::ServerPacket* GameMap::get_net_chunk(ChunkID const& chunk_id, uint64_t timestamp)
{
	//Use const accessor first to avoid exclusive locking
	const_accessor acc;
	get_surface_chunk_buffer(acc, chunk_id);
	if(acc->second->last_modified() <= timestamp ||
		acc->second->empty_surface() )
		return NULL;

	auto packet = new Network::ServerPacket();
	auto chunk = packet->mutable_chunk_response();
	
	if(!acc->second->serialize_to_protocol_buffer(*chunk))
	{
		//If this fails because the buffer is not ready, we resort to exclusive
		//locking in order to generate the cache
		acc.release();
		
		accessor mut_acc;
		get_surface_chunk_buffer(mut_acc, chunk_id);
		mut_acc->second->cache_protocol_buffer_data();
		mut_acc->second->serialize_to_protocol_buffer(*chunk);
	}
	else
	{	
		acc.release();
	}
	
	//Set chunk index
	chunk->set_x(chunk_id.x);
	chunk->set_y(chunk_id.y);
	chunk->set_z(chunk_id.z);
	
	return packet;
}

};
