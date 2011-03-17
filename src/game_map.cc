#include <sstream>
#include <cstdlib>
#include <stdint.h>

#include <tbb/scalable_allocator.h>
#include <tbb/atomic.h>
#include <tbb/compat/thread>
#include <tbb/spin_rw_mutex.h>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/concurrent_hash_map.h>

#include "constants.h"
#include "misc.h"
#include "config.h"
#include "chunk.h"
#include "game_map.h"


using namespace tbb;
using namespace std;

#define MAP_DB_DEBUG 1

#ifndef MAP_DB_DEBUG
#define DEBUG_PRINTF(...)
#else
#define DEBUG_PRINTF(...)  fprintf(stderr,__VA_ARGS__)
#endif


namespace Game
{

	

//-------------------------------------------------------------------
// Constructor/destructor pairs
//-------------------------------------------------------------------

GameMap::GameMap(Config* cfg) : 
	world_gen(new WorldGen(cfg)), 
	config(cfg),
	chunks(config->readInt("num_chunk_buckets")),
	surface_chunks(config->readInt("num_surface_chunk_buckets"))
{
	initialize_db();
}

GameMap::~GameMap()
{
	//Close database
	shutdown_db();
	
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

//-------------------------------------------------------------------
// Chunk accessors
//-------------------------------------------------------------------

//Retrieves a mutable accessor to the chunk buffer
void GameMap::get_chunk_buffer(accessor& acc, ChunkID const& chunk_id)
{
	if(!chunks.find(acc, chunk_id))
	{
		if(chunks.insert(acc, chunk_id))
		{
			generate_chunk(acc, chunk_id);
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
			generate_chunk(mut_acc, chunk_id);
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

//-------------------------------------------------------------------
// Single block accessors
//-------------------------------------------------------------------

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
	ChunkID chunk_id(x/CHUNK_X, y/CHUNK_Y, z/CHUNK_Z);

	//FIXME: Lock surface chunks only if t is a transparent block

	//Update chunk
	accessor acc;
	get_chunk_buffer(acc, chunk_id);
	if(acc->second->set_block(b, x%CHUNK_X, y%CHUNK_Y, z%CHUNK_Z, t))
	{
		return true;
	}
	
	mark_dirty(chunk_id);
	
	//FIXME: Update surface chunks here
	
	return false;
}

//-------------------------------------------------------------------
// Chunk accessors
//-------------------------------------------------------------------

//Chunk copying methods
void GameMap::get_chunk(ChunkID const& chunk_id, Block* buffer, int stride_x,  int stride_xz)
{
	const_accessor acc;
	get_chunk_buffer(acc, chunk_id);
	acc->second->decompress_chunk(buffer, stride_x, stride_xz);
}

//Retrieves a chunk protocol buffer
Network::Chunk* GameMap::get_chunk_pbuffer(ChunkID const& chunk_id)
{
	auto chunk = new Network::Chunk();
	chunk->set_x(chunk_id.x);
	chunk->set_y(chunk_id.y);
	chunk->set_z(chunk_id.z);
	
	//Use const accessor first to avoid exclusive locking
	const_accessor acc;
	get_chunk_buffer(acc, chunk_id);
	
	if(!acc->second->serialize_to_protocol_buffer(*chunk))
	{
		acc.release();
		
		accessor mut_acc;
		get_chunk_buffer(mut_acc, chunk_id);
		mut_acc->second->cache_protocol_buffer_data();
		mut_acc->second->serialize_to_protocol_buffer(*chunk);
	}
	else
	{	
		acc.release();
	}
	
	return chunk;
}


//Protocol buffer methods
Network::ServerPacket* GameMap::get_net_chunk(ChunkID const& chunk_id, uint64_t timestamp)
{
	//Use const accessor first to avoid exclusive locking
	const_accessor acc;
	get_surface_chunk_buffer(acc, chunk_id);
	if(acc->second->last_modified() <= timestamp ||
		acc->second->empty_surface() )
	{
		return NULL;
	}

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

//-------------------------------------------------------------------
// Chunk generation
//-------------------------------------------------------------------

//Generates a chunk, if it exists
void GameMap::generate_chunk(accessor& acc, ChunkID const& chunk_id)
{
	Block buffer[CHUNK_SIZE];
	world_gen->generate_chunk(chunk_id, buffer, CHUNK_X, CHUNK_X*CHUNK_Y);
	
	acc->second = new ChunkBuffer();
	acc->second->compress_chunk(buffer, CHUNK_X, CHUNK_X*CHUNK_Y);
	
	mark_dirty(chunk_id);
}

//Generates a surface chunk and stores it in the cache
//FIXME:  This would be more efficient if it directly operated on the compressed chunks. -Mik
void GameMap::generate_surface_chunk(accessor& acc, ChunkID const& chunk_id)
{
	const static int stride_x  = 3 * CHUNK_X;
	const static int stride_xz = 3 * stride_x * CHUNK_Z;
	const static int delta[][3] =	//Lock order: y, z, x, low to high
	{
		{ 0,-1, 0},
		{ 0, 0,-1},
		{-1, 0, 0},
		{ 0, 0, 0},
		{ 1, 0, 0},
		{ 0, 0, 1},
		{ 0, 1, 0}
	};
	
	//Lock the neighboring chunks and cache them
	const_accessor chunks[7];
	Block* buffer = (Block*)scalable_malloc(sizeof(Block)*CHUNK_SIZE*3*3*3);
	uint64_t timestamp = 1;	

	//Lock the surrounding buffers for reading, always use order y-z-x
	for(int i=0; i<7; ++i)
	{
		get_chunk_buffer(chunks[i],
						ChunkID(chunk_id.x+delta[i][0],
								chunk_id.y+delta[i][1],
								chunk_id.z+delta[i][2]));

		int ix = (delta[i][0]+1) * CHUNK_X,
			iy = (delta[i][1]+1) * CHUNK_Y,
			iz = (delta[i][2]+1) * CHUNK_Z;
			
		Block* ptr = buffer + ix + iz * stride_x + iy * stride_xz;
		chunks[i]->second->decompress_chunk(ptr, stride_x, stride_xz);
		timestamp = max(timestamp, chunks[i]->second->last_modified());
	}
	
	//Traverse to find surface chunks
	int i  = CHUNK_X + CHUNK_Z * stride_x + CHUNK_Y * stride_xz;
	const int
		dy = stride_xz - CHUNK_Z*stride_x,
		dz = stride_x  - CHUNK_X,
		dx = 1;
		
	bool empty = true;
	
	for(int y=0; y<CHUNK_Y; ++y, i+=dy)
	for(int z=0; z<CHUNK_Z; ++z, i+=dz)
	for(int x=0; x<CHUNK_X; ++x, i+=dx)
	{
		/*
		assert(i == (CHUNK_X + x) +
					(CHUNK_Z + z) * stride_x +
					(CHUNK_Y + y) * stride_xz );
		*/
	
		if( buffer[i].transparent() ||
			buffer[i-1].transparent() ||
			buffer[i+1].transparent() ||
			buffer[i-stride_x].transparent() ||
			buffer[i+stride_x].transparent() ||
			buffer[i-stride_xz].transparent() ||
			buffer[i+stride_xz].transparent() )
		{
			if(empty && buffer[i].type() != BlockType_Air)
			{
			/*
				DEBUG_PRINTF("Flagging non-empty, i:%d, cell: %d,%d,%d; value = %d, neighbors = %d,%d,%d,%d,%d,%d\n", i, x, y, z, buffer[i].int_val,
					buffer[i-1].int_val,
					buffer[i+1].int_val,
					buffer[i-stride_x].int_val,
					buffer[i+stride_x].int_val,
					buffer[i-stride_xz].int_val,
					buffer[i+stride_xz].int_val);
			*/
				empty = false;
			}
			continue;
		}
		
		buffer[i] = BlockType_Stone;
	}
	
	acc->second->compress_chunk(buffer + CHUNK_X + CHUNK_Z * stride_x + CHUNK_Y * stride_xz, stride_x, stride_xz);
	acc->second->set_last_modified(timestamp);
	acc->second->set_empty_surface(empty);
	if(!empty)
		acc->second->cache_protocol_buffer_data();
	
	//Release locks in reverse order
	for(int i=6; i>=0; --i)
	{
		chunks[i].release();
	}
	
	scalable_free(buffer);
}

//-------------------------------------------------------------------
// Persistence/disk IO routines
//-------------------------------------------------------------------

void GameMap::initialize_db()
{

	//Initialize the map database
	map_db = tchdbnew();
	
	//Set options
	//tchdbsetmutex(map_db);	//Not needed, will use our own mutex to synchronize database
	tchdbtune(map_db,
		config->readInt("tc_map_buckets"),				//Number of buckets
		config->readInt("tc_map_alignment"),			//Record alignment
		config->readInt("tc_map_free_pool_size"),		//Memory pool size
		HDBTLARGE | HDBTDEFLATE);

	tchdbsetcache(map_db, config->readInt("tc_map_cache_size"));
	tchdbsetxmsiz(map_db, config->readInt("tc_map_extra_memory"));

	//Open the map database
	tchdbopen(map_db, config->readString("map_db_path").c_str(), HDBOWRITER | HDBOCREAT);
	
	//Iterate over all previous map entries and cache them
	tchdbiterinit(map_db);
	
	//Restore the state of the map
	auto key = tcxstrnew();
	auto value = tcxstrnew();
	
	printf("Loading chunks");

	while(true)
	{
		if(!tchdbiternext3(map_db, key, value))
			break;
		
		ScopeDelete<Network::Chunk> pbuffer(new Network::Chunk());
		pbuffer.ptr->ParseFromArray(tcxstrptr(value), tcxstrsize(value));
		
		ChunkID chunk_id(pbuffer.ptr->x(), pbuffer.ptr->y(), pbuffer.ptr->z());
		
		auto chunk_buffer = new ChunkBuffer();
		chunk_buffer->parse_from_protocol_buffer(*pbuffer.ptr);
		
		accessor acc;
		chunks.insert(acc, make_pair(chunk_id, chunk_buffer) );
		
		printf(".");
	}
	
	tcxstrdel(key);
	tcxstrdel(value);
	
	printf("Done!\n");
	
	//Worker thread, this operates in the background and constantly writes updated chunks to the database
	struct DBWorker
	{
		GameMap* game_map;
		
		void operator()()
		{
			//Static buffer object
			uint8_t	buffer[(sizeof(Block) + 2) * CHUNK_SIZE + 256];
		
			while(game_map->running)
			{
				write_set_t	pending(256);
				{
					spin_rw_mutex::scoped_lock L(game_map->write_set_lock, true);
					pending.swap(game_map->pending_writes);
				}
	
				for(auto iter=pending.begin(); iter!=pending.end(); ++iter)
				{
					auto key = iter->first;
					
					//DEBUG_PRINTF("Serializing chunk: %d,%d,%d\n", key.x, key.y, key.z);
					
					//Serialize the protocol buffer to an array
					auto pbuffer = game_map->get_chunk_pbuffer(key);
					int bs = pbuffer->ByteSize();
					pbuffer->SerializeToArray(buffer, sizeof(buffer));
					delete pbuffer;
					
					//Store in tokyo cabinet
					uint32_t arr[3];
					arr[0] = key.x;
					arr[1] = key.y;
					arr[2] = key.z;
					tchdbput(game_map->map_db, (void*)arr, sizeof(arr), buffer, bs);
				}
				
				//Sleep
				this_thread::sleep_for(tick_count::interval_t((double)game_map->config->readFloat("map_db_write_rate")));
			}
		}
	};
	
	//Spawn the worker thread
	running = true;
	
	db_worker_thread = new thread((DBWorker){this});
}

void GameMap::shutdown_db()
{
	running = false;
	
	assert(db_worker_thread->joinable());
	db_worker_thread->join();
	delete db_worker_thread;

	tchdbclose(map_db);
	tchdbdel(map_db);
}

//Marks a chunk for disk serialization
void GameMap::mark_dirty(ChunkID const& chunk_id)
{
	spin_rw_mutex::scoped_lock L(write_set_lock, false);
	pending_writes.insert(make_pair(chunk_id, true));
}


};
