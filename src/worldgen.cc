
#include "constants.h"
#include "chunk.h"
#include "noise.h"

#include <string.h>

using namespace tbb;
using namespace std;

namespace Game
{

WorldGen::WorldGen(Config* cfg) : config(cfg)
{
}


void WorldGen::generate_chunk(ChunkID const& chunk_id, Block* data, int stride_x, int stride_xy)
{
	for(int k=0; k<CHUNK_Z; ++k)
	for(int j=0; j<CHUNK_Y; ++j)
	for(int i=0; i<CHUNK_X; ++i)
	{
		//Get block coordinate
		int x = chunk_id.x*CHUNK_X + i,
			y = chunk_id.y*CHUNK_Y + j,
			z = chunk_id.z*CHUNK_Z + k;
			
		//Compute pointer
		auto ptr = data + i + j * stride_x + k * stride_xy;
			
		//Determine block type
		if(y > (1<<20))
			*ptr = BlockType_Air;
		else if(y == (1<<20))
			*ptr = BlockType_Grass;
		else
			*ptr = BlockType_Stone;
	}
}

};

