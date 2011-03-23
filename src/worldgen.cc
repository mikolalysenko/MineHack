#include <stdint.h>
#include <cstdlib>
#include <cstdio>
#include <cmath>

#include <tbb/task.h>

#include "constants.h"
#include "config.h"
#include "chunk.h"
#include "noise.h"
#include "worldgen.h"

using namespace tbb;
using namespace std;

//Uncomment this line to get dense logging for the world generator
#define GEN_DEBUG 1

#ifndef GEN_DEBUG
#define DEBUG_PRINTF(...)
#else
#define DEBUG_PRINTF(...)  fprintf(stderr,__VA_ARGS__)
#endif


namespace Game
{

WorldGen::WorldGen(Config* cfg) : config(cfg)
{
}

WorldGen::~WorldGen()
{
}

void WorldGen::generate_chunk(ChunkID const& chunk_id, Block* data, int stride_x, int stride_xz)
{
	DEBUG_PRINTF("Generating chunk: %d, %d, %d\n", chunk_id.x, chunk_id.y, chunk_id.z);

	for(int k=0; k<CHUNK_Y; ++k)
	for(int j=0; j<CHUNK_Z; ++j)
	for(int i=0; i<CHUNK_X; ++i)
	{
		//Get block coordinate
		int x = chunk_id.x*CHUNK_X + i,
			z = chunk_id.z*CHUNK_Z + j,
			y = chunk_id.y*CHUNK_Y + k;
			
		//Compute pointer
		auto ptr = data + i + j * stride_x + k * stride_xz;
		
		y += i + j;
			
		if(abs(x - ORIGIN_X) < 10 &&
			abs(z - ORIGIN_Z) < 10 &&
			abs(y - ORIGIN_Y - 10) < 10)
		{
			*ptr = BlockType_Wood;
			continue;
		}	
			
			
		//Determine block type
		if(y > ORIGIN_Y)
			*ptr = BlockType_Air;
		else if(y == ORIGIN_Y)
			*ptr = BlockType_Grass;
		else
			*ptr = BlockType_Stone;
	}
}

};

