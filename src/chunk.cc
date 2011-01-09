#include "chunk.h"

using namespace std;


namespace Game
{

//Just interleaves the bits for each of the coordinates, giving z-order indexing
//Future idea: switch to Hilbert order curve
uint64_t ChunkId::hash() const
{
	static const uint64_t B[] = 
	{
		0xFFFF00000000FFFFULL,
		0x00FF0000FF0000FFULL,
		0xF00F00F00F00F00FULL,
		0x30c30c30c30c30c3ULL,
		0x9249249249249249ULL
	};
	
	uint64_t bx = x, by = y, bz = z;
	
	for(uint64_t i=48, j=0; i>0; i>>=1, ++j)
	{
		bx = (bx | (bx << i)) & B[j];
		by = (by | (by << i)) & B[j];
		bz = (bz | (bz << i)) & B[j];
	}
	
	return bx | (by << 1) | (bz << 2);
}


//Compresses a chunk for serialization
int Chunk::compress(void* buffer, size_t len)
{
	auto data_ptr = data[0][0];
	auto buf_ptr = (uint8_t*) buffer;
	size_t n = 0;
	
	for(size_t i=0; i<(CHUNK_X)*(CHUNK_Y)*(CHUNK_Z); )
	{
		Block cur = *data_ptr;
		
		size_t l;
		for(l=1; 
			i+l<(CHUNK_X)*(CHUNK_Y)*(CHUNK_Z) && 
			data_ptr[l] == cur; ++l)
		{
		}
		
		i += l;
		data_ptr += l;
		
		//Runs < 256 long get single byte encoded
		if(l < 256)
		{
			n += 2;
			if(n >= len)
				return -1;
			
			*(buf_ptr++) = (uint8_t)(l-1);
			*(buf_ptr++) = (uint8_t)cur;
			
		}
		//Runs >= 256 get 2-byte encoded
		else
		{
			n += 4;
			if(n >= len)
				return -1;
				
			*(buf_ptr++) = 0xff;
			*(buf_ptr++) = (uint8_t)(l&0xff);
			*(buf_ptr++) = (uint8_t)(l>>8);
			*(buf_ptr++) = (uint8_t)cur;
		}
	}
	
	return n;
}


//Decompress a chunk from a buffer
Chunk* Chunk::decompress(void* buffer, size_t len)
{
	return NULL;
}

};
