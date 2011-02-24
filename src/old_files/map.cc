#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <map>
#include <string>

#include <cstring>
#include <cstdlib>
#include <cstdint>

#include <tcutil.h>
#include <tchdb.h>


#include "misc.h"
#include "map.h"

using namespace std;


namespace Game
{

Map::Map(WorldGen* gen, string const& filename) : world_gen(gen)
{
	//Initialize the map database
	map_db = tchdbnew();
	
	//Set options
	tchdbsetmutex(map_db);
	tchdbtune(map_db,
		0,							//Number of buckets
		16,							//Record alignment
		10,							//Free elements
		HDBTLARGE | HDBTBZIP);
	tchdbsetcache(map_db,
		1024);
	
	//Open the map database
	tchdbopen(map_db, filename.c_str(), HDBOWRITER | HDBOCREAT);
	
	//Create the lock for the visibility buffers
	pthread_mutex_init(&vis_lock, NULL);
}

Map::~Map()
{
	tchdbclose(map_db);
	tchdbdel(map_db);
}

//Retrieves a particular chunk from the map
void Map::get_chunk(ChunkID const& idx, Chunk* chunk)
{
	int l = tchdbget3(map_db, 
		(const void*)&idx, sizeof(ChunkID),
		(void*)chunk, sizeof(Chunk));
	

	//If the value of l was not set correctly, need to regenerate the chunk
	if(l != sizeof(Chunk))
	{
		printf("Generating chunk: %d, %d, %d\n", idx.x, idx.y, idx.z);
	
		//Generate the chunk
		world_gen->generate_chunk(idx, chunk);
		
		//Store chunk in database
		if(!tchdbputkeep(map_db,
			(const void*)&idx, sizeof(ChunkID),
			(const void*)chunk, sizeof(Chunk)))
		{
			tchdbget3(map_db, 
				(const void*)&idx, sizeof(ChunkID),
				(void*)chunk, sizeof(Chunk));
		}
	}
}

//Retrieves a particular chunk from the map
bool Map::get_surface_chunk(ChunkID const& c, Chunk* chunk)
{
	Chunk center, left, right, top, bottom, front, back;
	
	bool empty = true;
	
	get_chunk(c, &center);
	get_chunk(ChunkID(c.x-1, c.y, c.z), &left);
	get_chunk(ChunkID(c.x+1, c.y, c.z), &right);
	get_chunk(ChunkID(c.x, c.y-1, c.z), &bottom);
	get_chunk(ChunkID(c.x, c.y+1, c.z), &top);
	get_chunk(ChunkID(c.x, c.y, c.z-1), &front);
	get_chunk(ChunkID(c.x, c.y, c.z+1), &back);
	
	for(int iz=0; iz<CHUNK_Z; ++iz)
	for(int iy=0; iy<CHUNK_Y; ++iy)
	for(int ix=0; ix<CHUNK_X; ++ix)
	{
		int idx = ix + (iy<<CHUNK_X_S) + (iz<<CHUNK_XY_S);
		
		//Read in current cell
		Block b = center.data[idx], ob;
		
		if(BLOCK_TRANSPARENCY[(int)b])
		{
			chunk->data[idx] = b;
			
			if(b != Block::Air)
			{
				empty = false;
			}
			
			continue;
		}
	
		//Check for surface block
		if(ix > 0)
		{
			ob = center.data[idx - 1];
		}
		else
		{
			ob = left.data[CHUNK_X - 1 + (iy << CHUNK_X_S) + (iz << CHUNK_XY_S)];
		}
		if(BLOCK_TRANSPARENCY[(int)ob])
		{
			empty = false;
			chunk->data[idx] = b;
			continue;
		}
				
		if(ix < CHUNK_X - 1)
		{
			ob = center.data[idx + 1];
		}
		else
		{
			ob = right.data[(iy << CHUNK_X_S) + (iz << CHUNK_XY_S)];
		}
		if(BLOCK_TRANSPARENCY[(int)ob])
		{
			empty = false;
			chunk->data[idx] = b;
			continue;
		}
		
		if(iy > 0)
		{
			ob = center.data[idx - CHUNK_X];
		}
		else
		{
			ob = bottom.data[ix + ((CHUNK_Y-1) << CHUNK_X_S) + (iz << CHUNK_XY_S)];
		}
		if(BLOCK_TRANSPARENCY[(int)ob])
		{
			empty = false;
			chunk->data[idx] = b;
			continue;
		}
		
		if(iy < CHUNK_Y - 1)
		{
			ob = center.data[idx + CHUNK_X];
		}
		else
		{
			ob = top.data[ix + (iz << CHUNK_XY_S)];
		}
		if(BLOCK_TRANSPARENCY[(int)ob])
		{
			chunk->data[idx] = b;
			continue;
		}
		
		if(iz > 0)
		{
			ob = center.data[idx - CHUNK_X*CHUNK_Y];
		}
		else
		{
			ob = front.data[ix + (iy << CHUNK_X_S) + ((CHUNK_Z-1) << CHUNK_XY_S)];
		}
		if(BLOCK_TRANSPARENCY[(int)ob])
		{
			empty = false;
			chunk->data[idx] = b;
			continue;
		}
				
		if(iz < CHUNK_Z - 1)
		{
			ob = center.data[idx + CHUNK_X*CHUNK_Y];
		}
		else
		{
			ob = back.data[ix + (iy << CHUNK_X_S)];
		}
		if(BLOCK_TRANSPARENCY[(int)ob])
		{
			empty = false;
			chunk->data[idx] = b;
			continue;
		}
		
		chunk->data[idx] = Block::Stone;
	}
	
	return empty;
}


//Sets a block in the map
//Note: this method is not efficient.  Most big map updates should get
// done in the physics worker group.
void Map::set_block(int x, int y, int z, Block b)
{
	auto idx = COORD2CHUNKID(x, y, z);
	Chunk c;
	
	bool commit = false;

	while(true)
	{
		if(!tchdbtranbegin(map_db))
			break;
	
		int l = tchdbget3(map_db, 
			(const void*)&idx, sizeof(ChunkID),
			(void*)&c, sizeof(Chunk));
		
		if(l != sizeof(Chunk))
		{
			tchdbtranabort(map_db);
			break;
		}
		
		//Don't do redundant sets
		if(c.data[CHUNK_OFFSET(x & CHUNK_X_MASK, 
							y & CHUNK_Y_MASK, 
							z & CHUNK_Z_MASK)] == b)
		{
			tchdbtranabort(map_db);
			break;
		}

		c.data[CHUNK_OFFSET(x & CHUNK_X_MASK, 
							y & CHUNK_Y_MASK, 
							z & CHUNK_Z_MASK)] = b;
						
		tchdbput(map_db,
			(const void*)&idx, sizeof(ChunkID),
			(void*)&c, sizeof(Chunk));
		
		if(tchdbtrancommit(map_db))
		{
			commit = true;
			break;
		}
	}
	
	//If block was successfully updated, invalidate the visibility buffer for this chunk	
	if(commit)
	{
		invalidate_vis_buffer(pos_to_vis_key(x, y, z));
	}
}

//Retrieves a chunk from the map
Block Map::get_block(int x, int y, int z)
{
	Chunk c;	
	get_chunk(COORD2CHUNKID(x,y,z), &c);
	
	return c.data[CHUNK_OFFSET(x & CHUNK_X_MASK, 
								y & CHUNK_Y_MASK, 
								z & CHUNK_Z_MASK)];
}

//Convert a position to a bucket
uint64_t pos_to_vis_key(double x, double y, double z)
{
	return chunkid_to_vis_key(ChunkID(x / CHUNK_X, y / CHUNK_Y, z / CHUNK_Z));
}

//Converts a chunk ID to a key
uint64_t chunkid_to_vis_key(ChunkID const& c)
{
	auto k =
		 (uint64_t)(c.x/VIS_BUFFER_X)    |
		((uint64_t)(c.y/VIS_BUFFER_Y)<<20ULL) |
		((uint64_t)(c.z/VIS_BUFFER_Z)<<40ULL);
	return k;
}

//Retrieves a buffer containing all visible chunks
int Map::send_vis_buffer(uint64_t key, int socket)
{
	VisBuffer buf;
	
	{
		MutexLock L(&vis_lock);
		auto iter = vis_buffers.find(key);
	
		if(iter == vis_buffers.end() || 
			(*iter).second.ptr == NULL)
		{
			printf("Generating buffer\n");
			gen_vis_buffer(key, true);
			printf("Gen complete\n");
		}
		

		buf = vis_buffers[key];
	}
	
	if(buf.size == 0)
	{
		return 0;
	}
	
	return send(socket, buf.ptr, buf.size, 0);
}
	
//Generates a visibility buffer
void Map::gen_vis_buffer(uint64_t key, bool no_lock)
{
	//Check if buffer already exists
	if(no_lock)
	{
		auto iter = vis_buffers.find(key);
		if(iter != vis_buffers.end())
		{
			if((*iter).second.ptr == NULL)
			{
				vis_buffers.erase(iter);
			}
		}
	}
	else
	{
		MutexLock L(&vis_lock);
		auto iter = vis_buffers.find(key);
		if(iter != vis_buffers.end())
		{
			//If this is set, then the buffer is just a place holder
			if((*iter).second.ptr == NULL)
			{
				vis_buffers.erase(iter);
			}
			else
			{
				return;
			}
		}
	}

	//First compute the base chunk
	ChunkID base_chunk(
		(key&((1<<20)-1)) * VIS_BUFFER_X,
		((key>>20)&((1<<20)-1)) * VIS_BUFFER_Y,
		((key>>40)&((1<<20)-1)) * VIS_BUFFER_Z);
	
	//Next, allocate temporary buffer
	ScopeFree buf(malloc(9 + (2 + 2*sizeof(Block)*CHUNK_X*CHUNK_Y*CHUNK_Z)*VIS_BUFFER_X*VIS_BUFFER_Y*VIS_BUFFER_Z));

	//Loop through all chunks in block
	uint8_t *chunk_ptr = ((uint8_t*)buf.ptr) + 9 + 2*VIS_BUFFER_X*VIS_BUFFER_Y*VIS_BUFFER_Z;
	uint8_t *coord_ptr = chunk_ptr;
	int n_chunks = 0;
	Chunk chunk;
	for(int x=base_chunk.x; x<base_chunk.x + VIS_BUFFER_X; ++x)
	for(int y=base_chunk.y; y<base_chunk.y + VIS_BUFFER_Y; ++y)
	for(int z=base_chunk.z; z<base_chunk.z + VIS_BUFFER_Z; ++z)
	{
		if(get_surface_chunk(ChunkID(x,y,z), &chunk))
			continue;
			
		int c_size = chunk.compress(chunk_ptr, 2*sizeof(Chunk));
		assert(c_size > 0);
		chunk_ptr += c_size;
		
		
		//Store coordinate offset
		*(--coord_ptr) = 
			(x-base_chunk.x) +
			((y-base_chunk.y)<<3) +
			((z-base_chunk.z)<<5);
			
		++n_chunks;
	}
	
	//Special case: No chunks in buffer
	if(n_chunks == 0)
	{
		if(no_lock)
		{
			vis_buffers[key] = VisBuffer{ malloc(1), 0 };
		}
		else
		{
			MutexLock L(&vis_lock);
			auto iter = vis_buffers.find(key);
			if(iter == vis_buffers.end())
			{
				vis_buffers[key] = VisBuffer{ malloc(1), 0 };
			}
		}
		
		return;
	}
	
	//Reverse offsets
	for(int i=0; i<(n_chunks>>1); ++i)
	{
		uint8_t tmp = coord_ptr[i];
		coord_ptr[i] = coord_ptr[n_chunks - 1 - i];
		coord_ptr[n_chunks - 1 - i] = tmp;
	}
	
	//Write base offset
	*(--coord_ptr) = n_chunks;
	*((uint64_t*)(coord_ptr)-1) = key;
	
	//Compute offset of base pointer and length
	void* base_ptr = (void*)(coord_ptr - sizeof(uint64_t));
	int len = chunk_ptr - (uint8_t*)base_ptr;
	
	
	//Generate a compressed buffer
	int sz;
	ScopeFree compressed_chunks((void*)tcdeflate((const char*)base_ptr, len, &sz));
	
	void* compressed_vis_buffer = malloc(sz + 1024);
	
	int delta = sprintf((char*)compressed_vis_buffer, 
      "HTTP/1.1 200 OK\n"
	  "Cache: no-cache\n"
	  "Content-Type: application/octet-stream; charset=x-user-defined\n"
	  "Content-Transfer-Encoding: binary\n"
	  "Content-Encoding: deflate\n"
	  "Content-Length: %d\n"
	  "\n", sz);

	//Append result
	memcpy((uint8_t*)compressed_vis_buffer+delta, compressed_chunks.ptr, sz);
	
	//Create vis buffer
	VisBuffer vb { compressed_vis_buffer, sz+delta };
	
	//Check if buffer changed
	if(no_lock)
	{
		vis_buffers[key] = vb;
	}
	else
	{
		bool success = false;
		{
			MutexLock L(&vis_lock);
			auto iter = vis_buffers.find(key);
			if(iter == vis_buffers.end())
			{
				success = true;
				vis_buffers[key] = vb;
			}
		}
		
		//If we failed to generate the visibility buffer, then die
		if(!success)
		{
			free(compressed_vis_buffer);
		}
	}
}

//Invalidates a visibility buffer after a chunk changes
void Map::invalidate_vis_buffer(uint64_t key)
{
	MutexLock L(&vis_lock);
	
	auto iter = vis_buffers.find(key);
	if(iter == vis_buffers.end())
	{
		//Vis buffer doesn't exist, yet.  Add bogus vis buffer and exit.
		vis_buffers[key] = VisBuffer{NULL, 0};
		return;
	}
	
	auto vb = (*iter).second;
	
	if(vb.ptr == NULL)
	{
		//Already invalidated
	}
	else
	{
		//Mark the vis buffer for regeneration
		free(vb.ptr);
		vb.ptr = NULL;
		vb.size = 0;
		vis_buffers[key] = vb;
	}
}


};
