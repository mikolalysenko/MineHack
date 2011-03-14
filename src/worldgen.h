#ifndef WORLD_H
#define WORLD_H

#include <cstdint>

#include "constants.h"
#include "config.h"
#include "chunk.h"

namespace Game
{

	//The world implements a set of rules for generating chunks
	struct WorldGen
	{
		WorldGen(Config* cfg);
		
		//Generates a chunk in place
		void generate_chunk(ChunkID const&, Block* data, int stride_x, int stride_xy);
		
	private:
		Config *config;
	};
};


#endif
