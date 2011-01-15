#ifndef WORLD_H
#define WORLD_H

#include "chunk.h"
#include "config.h"

//padding around the current block to generate surface data
#define SURFACE_GEN_PADDING     3

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
		WorldGen(Config* mapconfig);
		
		void generate_chunk(ChunkID const&, Chunk*);
		
		Block generate_block(int x, int y, int z, SurfaceCell surface);
		
		private:
			
			SurfaceCell generate_surface_data(int, int);
			bool near_water(int x, int y, SurfaceCell surface[CHUNK_Z + (SURFACE_GEN_PADDING << 1)][CHUNK_X + (SURFACE_GEN_PADDING << 1)]);
	};
};


#endif
