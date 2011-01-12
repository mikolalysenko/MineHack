#ifndef WORLD_H
#define WORLD_H

#include "chunk.h"

namespace Game
{
	//The world implements a set of rules for generating chunks
	struct WorldGen
	{
		void generate_chunk(ChunkID const&, Chunk*);
		
		Block generate_block(int x, int y, int z);
	};
};


#endif
