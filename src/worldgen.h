#ifndef WORLD_H
#define WORLD_H

#include "chunk.h"

namespace Game
{
	struct SurfaceCell
	{
		int height;
		bool nearWater;
	};
	
	//The world implements a set of rules for generating chunks
	struct WorldGen
	{
		void generate_chunk(ChunkID const&, Chunk*);
		
		Block generate_block(int x, int y, int z, SurfaceCell surface);
		
		SurfaceCell generate_surface_data(int, int);
	};
};


#endif
