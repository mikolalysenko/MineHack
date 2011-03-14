#ifndef WORLDGGEN_H
#define WORLDGEN_H

#include <stdint.h>

#include "constants.h"
#include "config.h"
#include "chunk.h"

namespace Game
{

	//The world implements a set of rules for generating chunks
	struct WorldGen
	{
		WorldGen(Config* cfg);
		~WorldGen();
		
		//Generates a chunk in place
		//Interface was changed to accommodate new cellular automata physics system
		// and to allow in place chunk generation.
		//This method must be reentrant and thread safe.  (and hopefully fast)
		//It gets called *frequently*.
		void generate_chunk(ChunkID const&, Block* data, int stride_x, int stride_xy);
		
	private:
		Config *config;
	};
};


#endif
