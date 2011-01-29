#ifndef WORLD_H
#define WORLD_H

#include <cstdint>
#include "chunk.h"
#include "config.h"

//padding around the current block to generate surface data
#define SURFACE_GEN_PADDING     3

#define CAVE_SEGMENT_COUNT      5

namespace Game
{
	struct SurfaceCell
	{
		int height;
		bool nearWater;
	};
	
	struct CaveSegment
	{
		int64_t sx, sy, sz, ex, ey, ez;
		
		int64_t dist(int64_t px, int64_t py, int64_t pz);
	};
	
	struct Cave
	{
		bool used;
		CaveSegment segments[CAVE_SEGMENT_COUNT];
		
		int64_t dist(int64_t px, int64_t py, int64_t pz);
	};
	
	struct CavePoint
	{
		int64_t x, y, z;
	};
	
	struct CaveSystem
	{
		CavePoint points[3][3][3];
		Cave caves[54];
	};
	
	//The world implements a set of rules for generating chunks
	struct WorldGen
	{
		WorldGen(Config* mapconfig);
		
		void generate_chunk(ChunkID const&, Chunk*);
		
		Block generate_block(int64_t x, int64_t y, int64_t z, SurfaceCell surface, CaveSystem s);
		
		CaveSystem generate_local_caves(ChunkID const&);
		
		private:
			
			Cave generate_cave(CavePoint start, CavePoint end);
			bool cave_used(CavePoint start, CavePoint end);
			CavePoint generate_cave_point(int64_t scx, int64_t scy, int64_t scz);
			SurfaceCell generate_surface_data(int64_t, int64_t);
			bool near_water(int64_t x, int64_t y, SurfaceCell surface[CHUNK_Z + (SURFACE_GEN_PADDING << 1)][CHUNK_X + (SURFACE_GEN_PADDING << 1)]);
	};
};


#endif
