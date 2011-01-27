#include "chunk.h"

using namespace std;


namespace Game
{


//Block lighting parameters
const float BLOCK_REFLECTANCE[] =
{
	0, 		//Air
	0.8,	//Stone
	0.5,	//Dirt
	0.6,	//Grass
	0.65,	//Cobblestone
	0.6,	//Wood
	0.5,	//Log
	0.1,	//Water
	0.7		//Sand
};

const float BLOCK_TRANSMISSION[] =
{
	1.,		//Air
	0,		//Stone
	0,		//Dirt
	0,		//Grass
	0,		//Cobblestone
	0,		//Wood
	0,		//Log
	0.8,	//Water
	0		//Sand
};

const float BLOCK_EMISSIVITY[] = 
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
	size_t n = 1, i;
	
	if(len <= 1)
		return -1;
	
	//Write flags
	n = 1;
	*(buf_ptr++) = flags;
	
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
