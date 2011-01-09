#ifndef WORLD_H
#define WORLD_H

#include "chunk.h"

namespace Game
{
	//The world implements a set of rules for generating chunks
	struct WorldGen
	{
		Chunk* generate_chunk(const ChunkId&);
		
		Block generate_block(int x, int y, int z);
	};
};


#endif
