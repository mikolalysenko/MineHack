#ifndef WORLD_H
#define WORLD_H

#include <cstdint>
#include "chunk.h"
#include "config.h"
#include "cave.h"

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
		
		Block generate_block(int64_t x, int64_t y, int64_t z, SurfaceCell surface, CaveSystem s);
		
		private:
			SurfaceCell generate_surface_data(int64_t, int64_t);
			bool near_water(int64_t x, int64_t y, SurfaceCell surface[CHUNK_Z + (SURFACE_GEN_PADDING << 1)][CHUNK_X + (SURFACE_GEN_PADDING << 1)]);
	};
};


#endif
