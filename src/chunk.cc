#include <stdint.h>
#include <cstdlib>

#include <tbb/task.h>

#include "constants.h"
#include "misc.h"
#include "chunk.h"

using namespace std;
using namespace tbb;

#define CHUNK_DEBUG 1

#ifndef CHUNK_DEBUG
#define DEBUG_PRINTF(...)
#else
#define DEBUG_PRINTF(...)  fprintf(stderr,__VA_ARGS__)
#endif

namespace Game
{

//Block data
const bool BLOCK_TRANSPARENCY[] =
{
	true,		//Air
	false,		//Stone
	false,		//Dirt
	false,		//Grass
	false,		//Cobblestone
	false,		//Wood
	false,		//Log
	true,		//Water
	false		//Sand
};

//Block state bytes (can be from 0 to 3)
const int BLOCK_STATE_BYTES[] =
{
	0,		//Air
	0,		//Stone
	0,		//Dirt
	0,		//Grass
	0,		//Cobblestone
	0,		//Wood
	0,		//Log
	0,		//Water
	0		//Sand
};


//Checks if two blocks are equal
bool Block::operator==(const Block& other) const
{
	if(type != other.type) return false;
	for(int i=0; i<BLOCK_STATE_BYTES[type]; ++i)
	{
		if(state[i] != other.state[i])
		{
			return false;
		}
	}
	return true;
}

//Hashes chunk indices
size_t ChunkIDHashCompare::hash(const ChunkID& chunk_id) const
{
	uint64_t h = 0ULL;
	for(uint64_t i=0ULL; i<64ULL; i+=3)
	{
		h |= (((uint64_t)chunk_id.x&1ULL)<<i) +
			 (((uint64_t)chunk_id.y&1ULL)<<(i+1)) +
			 (((uint64_t)chunk_id.z&1ULL)<<(i+2));
	}
	return (size_t)h;
}


//Chunk compression
ChunkBuffer compress_chunk(Block* chunk, int stride_x, int stride_xz)
{
	uint8_t compress_buffer[CHUNK_SIZE * 5];

	auto buf_start	= compress_buffer;
	auto buf_ptr	= buf_start;
	auto data_ptr	= chunk;

	stride_xz -= stride_x * CHUNK_Z - 1;
	stride_x  -= CHUNK_X - 1;
	
	size_t i;
	for(i = 0; i<CHUNK_SIZE; )
	{
		Block cur = *data_ptr;
		
		size_t l;
		for(l=0; i<CHUNK_SIZE && *data_ptr == cur; ++l)
		{
			++i;
			if( i & (CHUNK_X - 1) )
				data_ptr += 1;
			else if( i & (CHUNK_X*CHUNK_Z - 1) )
				data_ptr += stride_x;
			else
				data_ptr += stride_xz;
		}
		
		//Write run length as a var int in little endian
		while(l > 0x80)
		{
			*(buf_ptr++) = (l & 0x7f) | 0x80;
			l >>= 7;
		}
		*(buf_ptr++) = l;
		
		//Write block
		*(buf_ptr++) = cur.type;
		for(int j=0; j<BLOCK_STATE_BYTES[cur.type]; ++j)
		{
			*(buf_ptr++) = cur.state[j];
		}
	}

	//Return resulting run-length encoded stream
	int buf_size = (int)(buf_ptr - buf_start);
	auto ptr = (uint8_t*)malloc(buf_size);
	memcpy(ptr, buf_start, buf_size);
	
	ChunkBuffer result;
	result.size = buf_size;
	result.last_modified = 0;
	result.data = ptr;
	return result;
}

//Decompresses a chunk
void decompress_chunk(ChunkBuffer const& buffer, Block* chunk, int stride_x, int stride_xz)
{
	assert(false);
}

};
