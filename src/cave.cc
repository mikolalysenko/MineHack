#include "cave.h"

#include "noise.h"

using namespace std;

namespace Game
{
	
#define CAVE_MASK_X ((1 << (CHUNK_X_S + CAVE_BOX_S)) - 1)
#define CAVE_MASK_Y ((1 << (CHUNK_Y_S + CAVE_BOX_S)) - 1)
#define CAVE_MASK_Z ((1 << (CHUNK_Z_S + CAVE_BOX_S)) - 1)

#define SEGMENT_OFFSET_MASK 0xf;

//this actually returns the distance squared, to avoid needing to calculate a square root
int64_t CaveSegment::dist(int64_t px, int64_t py, int64_t pz)
{
	int64_t dirx = ex - sx;
	int64_t diry = ey - sy;
	int64_t dirz = ez - sz;
	
	int64_t dx = sx - px;
	int64_t dy = sy - py;
	int64_t dz = sz - pz;
	
	int64_t tn = (dx * dirx) + (dy * diry) + (dz * dirz);
	int64_t td = (dirx * dirx) + (diry * diry) + (dirz * dirz);
	
	float t;
	
	if(td != 0)
		t = -(float)tn / (float)td;
	else
		t = 0;  //if td=0 then the two points are on top of each other. Should never happen, but if it does, just take the distance to one of the points
	
	if(t > 1)  //if the point is closest to the end point
		return ((px - ex) * (px - ex)) + ((py - ey) * (py - ey)) + ((pz - ez) * (pz - ez));
	if(t <= 0)
		return (dx * dx) + (dy * dy) + (dz * dz);
	
	int64_t cpx = (diry * dz) - (dirz * dy);
	int64_t cpy = (dirz * dx) - (dirx * dz);
	int64_t cpz = (dirx * dy) - (diry * dx);
	
	return ((cpx * cpx) + (cpy * cpy) + (cpz * cpz)) / td;
}

//this actually returns the distance squared, to avoid needing to calculate a square root
int64_t Cave::dist(int64_t px, int64_t py, int64_t pz)
{
	if(CAVE_SEGMENT_COUNT < 1)
		return 0;
	
	int64_t dist = segments[0].dist(px, py, pz);
	
	for(int x = 1; x < CAVE_SEGMENT_COUNT; x++)
		dist = min(dist, segments[x].dist(px, py, pz));
	
	return dist;
}

Cave::Cave(CavePoint start, CavePoint end)
{
	uint64_t rnd = pseudorand_var(3, start.x + end.x, start.y + end.y, start.z + end.z);
	if(rnd%12 < 5)
	{
		CavePoint points[CAVE_SEGMENT_COUNT + 1];
		
		points[0] = start;
		points[CAVE_SEGMENT_COUNT] = end;
		
		float t;
		
		for(int x = 1; x < CAVE_SEGMENT_COUNT; x++)
		{
			t = (float)x / (float)CAVE_SEGMENT_COUNT;
			points[x].x = start.x + (end.x - start.x) * t;
			points[x].y = start.y + (end.y - start.y) * t;
			points[x].z = start.z + (end.z - start.z) * t;
			
			points[x].x += pseudorand_var(3, points[x].x, points[x].y, points[x].z) & SEGMENT_OFFSET_MASK;
			points[x].y += pseudorand_var(3, points[x].y, points[x].z, points[x].x) & SEGMENT_OFFSET_MASK;
			points[x].z += pseudorand_var(3, points[x].z, points[x].x, points[x].y) & SEGMENT_OFFSET_MASK;
		}
		
		for(int x = 0; x < CAVE_SEGMENT_COUNT; x++)
		{
			segments[x].sx = points[x].x;
			segments[x].sy = points[x].y;
			segments[x].sz = points[x].z;
			
			segments[x].ex = points[x+1].x;
			segments[x].ey = points[x+1].y;
			segments[x].ez = points[x+1].z;
		}
		
		used = true;
	}
	else
	{
		used = false;
	}
}

Cave::Cave()
{
}
	
CavePoint::CavePoint(int64_t scx, int64_t scy, int64_t scz)
{
	x = (scx << (CHUNK_X_S + CAVE_BOX_S)) | (pseudorand_var(3, scx, scy, scz) & CAVE_MASK_X);
	y = (scy << (CHUNK_Y_S + CAVE_BOX_S)) | (pseudorand_var(3, scy, scz, scx) & CAVE_MASK_Y);
	z = (scz << (CHUNK_Z_S + CAVE_BOX_S)) | (pseudorand_var(3, scz, scx, scy) & CAVE_MASK_Z);
}

CavePoint::CavePoint()
{
}

bool CaveSystem::check_dist(int64_t px, int64_t py, int64_t pz, int64_t testdist)
{
	//check to see if this block is inside a cave
	for(int index = 0; index < 54; index++)
	{
		if(caves[index].used)
		{
			for(int seg = 0; seg < CAVE_SEGMENT_COUNT; seg++)
			{
				if(caves[index].dist(px, py, pz) < testdist)
					return true;
			}
		}
	}
	
	return false;
}

CaveSystem::CaveSystem(ChunkID const& idx)
{
	int64_t blockx = (uint64_t)(idx.x) >> CAVE_BOX_S;
	int64_t blocky = (uint64_t)(idx.y) >> CAVE_BOX_S;
	int64_t blockz = (uint64_t)(idx.z) >> CAVE_BOX_S;
	
	for(int z = -1; z <= 1; z++)
	{
		for(int y = -1; y <= 1; y++)
		{
			for(int x = -1; x <= 1; x++)
				points[z+1][y+1][x+1] = CavePoint(blockx + x, blocky + y, blockz + z);
		}
	}
	
	int index = 0;

	for(int k = 0; k <= 1; k++)
	{
		for(int j = -1; j <= 1; j++)
		{
			for(int i = -1; i <= 1; i++)
			{
				caves[index] = Cave(points[k][j+1][i+1], points[k+1][j+1][i+1]); index++;
				caves[index] = Cave(points[j+1][k][i+1], points[j+1][k+1][i+1]); index++;
				caves[index] = Cave(points[j+1][i+1][k], points[j+1][i+1][k+1]); index++;
			}
		}
	}
}

};