#include <tbb/task.h>

#include "chunk.h"

using namespace std;
using namespace tbb;

namespace Game
{

//Chunk compression buffers
uint8_t		chunk_compress_buffers[32][CHUNK_SIZE * 5];

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
bool operator==(const Block& other) const
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

size_t ChunkIDHashCompare::hash(const ChunkID& chunk_id) const
{
}


//Chunk compression
uint8_t* compress_chunk(Block* chunk, int stride_x, int stride_xy, int* size)
{
	auto buf_start	= chunk_compress_buffers[FIXME];
	auto buf_ptr	= buf_start;
	auto data_ptr	= chunk;
	
	size_t i;
	for(i = 0; i<CHUNK_SIZE; )
	{
		Block cur = *data_ptr;
		
		size_t l;
		for(l=0; i<CHUNK_SIZE && *data_ptr == cur; ++l)
		{
			++i;
			if((i & (CHUNK_X*CHUNK_Y-1))
				data_ptr += 1;
			else if( (i & (CHUNK_X-1) )
				data_ptr += stride_x;
			else
				data_ptr += stride_xy;
		}
		
		//Write run length as a var int in little endian
		for(size_t p=l; p>0x80; p>>=7)
		{
			*(buf_ptr++) = (p & 0x7f) | 0x80;
		}
		*(buf_ptr++) = l & 0x7f;
		
		//Write block
		*(buf_ptr++) = cur.type;
		for(int j=0; j<BLOCK_STATE_BYTES[cur.type]; ++j)
		{
			*(buf_ptr++) = cur.state[j];
		}
	}

	//Return resulting run-length encoded stream
	int buf_size = (int)(buf_ptr - buf_start);
	auto result = (uint8_t*)malloc(buf_size);
	memcpy(result, buf_start, buf_size);
	
	*size = buf_size;
	return result;
}

//Decompresses a chunk
void decompress_chunk(Block* chunk, int stride_x, int stride_xy, uint8_t* buffer)
{
	auto buf_ptr = buffer;
	auto data_ptr = chunk;
	size_t i = 0;
	
	while(i < CHUNK_SIZE)
	{
		//Decode size
		int l = 0;
		for(int j=0; j<32; j+=7)
		{
			uint8_t c = *(buf_ptr++);
			l += (int)(c & 0x7f) << j;
			if(c < 0x80)
				break;
		}
		
		//Decode block
		Block cur(*buf_ptr, buf_ptr+1);
		buf_ptr += BLOCK_STATE_BYTES[*buf_ptr] + 1;
		
		//Write block to stream
		for(int j=0; j<l; ++j)
		{
			*data_ptr = cur;
			
			++i;
			if((i & (CHUNK_X*CHUNK_Y-1))
				data_ptr += 1;
			else if( (i & (CHUNK_X-1) )
				data_ptr += stride_x;
			else
				data_ptr += stride_xy;
		}
	}
}

};
