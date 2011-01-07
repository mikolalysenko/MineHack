#include "map.h"

using namespace std;
using namespace MapData;


int Chunk::compress(void* buffer, size_t len)
{
	auto data_ptr = data[0][0];
	auto buf_ptr = (uint8_t*) buffer;
	auto eof_ptr = buf_ptr + len;
	
	for(size_t i=0; i<(CHUNK_X+2)*(CHUNK_Y+2)*(CHUNK_Z+2); )
	{
		Block cur = *data_ptr;
		
		size_t l;
		for(l=1; 
			l<256 && 
			i+l<(CHUNK_X+2)*(CHUNK_Y+2)*(CHUNK_Z+2) && 
			data_ptr[l] == cur; ++l)
		{
		}
		
		i += l;
		data_ptr += l;
	}
}


