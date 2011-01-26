#include "chunk.h"

using namespace std;


namespace Game
{

//Just interleaves the bits for each of the coordinates, giving z-order indexing
//Future idea: switch to Hilbert order curve
uint64_t ChunkID::hash() const
{
	return (uint64_t)x |
		   (((uint64_t)y)<<CHUNK_IDX_S) |
		   (((uint64_t)z)<<CHUNK_IDX_S);
}


//Compresses a chunk for serialization
int Chunk::compress(void* buffer, int len)
{
	auto data_ptr	= data;
	auto buf_ptr	= (uint8_t*) buffer;
	size_t n = 0, i;
	
	if(len <= 0)
		return -1;
	
	for(i=0; i<(CHUNK_X)*(CHUNK_Y)*(CHUNK_Z); )
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
		--l;
		if(l < 0xff)
		{
			n += 2;
			if(n > len)
				return -1;
			
			*(buf_ptr++) = (uint8_t)(l);
			*(buf_ptr++) = (uint8_t)cur;
			
		}
		//Runs >= 256 get 2-byte encoded
		else
		{
			n += 4;
			if(n > len)
				return -1;
				
			*(buf_ptr++) = 0xff;
			*(buf_ptr++) = (uint8_t)(l&0xff);
			*(buf_ptr++) = (uint8_t)(l>>8);
			*(buf_ptr++) = (uint8_t)cur;
		}
	}
	
	return n;
}

};
