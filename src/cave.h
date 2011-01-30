#ifndef CAVE_H
#define CAVE_H

#include <cstdint>
#include "chunk.h"

#define CAVE_SEGMENT_COUNT      5

#define CAVE_BOX_S				2

namespace Game
{
	
	struct CaveSegment;
	struct CavePoint;
	struct Cave;
	struct CaveSystem;
	
	struct CaveSegment
	{
		private:
			int64_t sx, sy, sz, ex, ey, ez;
			
		public:
			int64_t dist(int64_t px, int64_t py, int64_t pz);
		
		friend struct Cave;
	};

	struct CavePoint
	{
		private:
			int64_t x, y, z;
		
		public:
			CavePoint();
			CavePoint(int64_t scx, int64_t scy, int64_t scz);
		
		friend struct Cave;
	};
	
	struct Cave
	{
		private:
			bool used;
			CaveSegment segments[CAVE_SEGMENT_COUNT];
		
		public:
			int64_t dist(int64_t px, int64_t py, int64_t pz);
			Cave();
			Cave(CavePoint start, CavePoint end);
		
		friend struct CaveSystem;
	};
	
	struct CaveSystem
	{
		private:
			CavePoint points[3][3][3];
			Cave caves[54];
		
		public:
			bool check_dist(int64_t px, int64_t py, int64_t pz, int64_t testdist);
			CaveSystem(ChunkID const& idx);
	};


};

#endif