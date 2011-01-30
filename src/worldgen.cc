#include "chunk.h"
#include "world.h"
#include "noise.h"

#include <string.h>

using namespace std;


//level at which water is
#define WATER_LEVEL				(1<<20)

//height of the tallest possible mountain
#define WORLD_MAX_HEIGHT		(WATER_LEVEL+60)

//height of the lowest possible point of the ocean
#define WORLD_MIN_HEIGHT		(WATER_LEVEL-60)

//scalar for the noise function
#define NOISE_SCALAR    		256

//how fine grained the features on land are (higher = more fine grained, but slower performance)
#define OCTAVES                 4

#define CAVE_BOX_S				2

namespace Game
{

WorldGen::WorldGen(Config* mapconfig)
{
	int mapseed = mapconfig->readInt("mapseed");
	
	if(mapseed == 0)
	{
		srand(time(NULL));
		while(mapseed == 0)
		{
			mapseed = rand();
			
			if(mapseed != 0)
				mapconfig->storeInt(mapseed, "mapseed");
		}
	}
	
	setNoiseSeed(mapseed);
}

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

Block WorldGen::generate_block(int64_t x, int64_t y, int64_t z, SurfaceCell surface, CaveSystem cave)
{
	//early out for stuff over the surface, not needed for correct execution
	if(y > surface.height && y > WATER_LEVEL)
		return Block::Air;
	
	if(y <= surface.height)  //don't need to check caves in the water or air
	{
		//check to see if this block is inside a cave
		for(int index = 0; index < 54; index++)
		{
			if(cave.caves[index].used)
			{
				for(int seg = 0; seg < CAVE_SEGMENT_COUNT; seg++)
				{
					if(cave.caves[index].dist(x, y, z) < 16)
						return Block::Air;
				}
			}
		}
	}
	
	if(!surface.nearWater)
	{
		if(y == surface.height)
			return Block::Grass;
		
		if(y < surface.height && y >= surface.height - 4)
			return Block::Dirt;
	}
	else
	{
		if(y <= surface.height && y >= surface.height - 4)
			return Block::Sand;
	}
	
	if(y < surface.height)
	{
		double dx = x;
		double dy = y;
		double dz = z;
		
		dx /= 32;
		dy /= 32;
		dz /= 32;
		
		if(simplexNoise3D(dx, dy, dz, 4) < .3)
			return Block::Dirt;
		
		return Block::Stone;
	}
	
	if(y > WATER_LEVEL)
		return Block::Air;
	
	return Block::Water;
}

SurfaceCell WorldGen::generate_surface_data(int64_t cx, int64_t cy)
{
	SurfaceCell cell;
	
	float fxp = ((float)cx) / NOISE_SCALAR;
	float fyp = ((float)cy) / NOISE_SCALAR;
	
	cell.height = (simplexNoise2D(fxp, fyp, OCTAVES) * (WORLD_MAX_HEIGHT - WORLD_MIN_HEIGHT)) + WORLD_MIN_HEIGHT;
	
	return cell;
}

bool WorldGen::near_water(int64_t x, int64_t y, SurfaceCell surface[CHUNK_Z + (SURFACE_GEN_PADDING << 1)][CHUNK_X + (SURFACE_GEN_PADDING << 1)])
{
	for(int yp = y - 2; yp <= y + 2; yp++)
	{
		for(int xp = x - 2; xp <= x + 2; xp++)
		{
			if(surface[yp][xp].height <= WATER_LEVEL)
				return true;
		}
	}
	
	return false;
}

#define SEGMENT_OFFSET_MASK 0xf;

Cave WorldGen::generate_cave(CavePoint start, CavePoint end)
{
	Cave c;
	
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
			c.segments[x].sx = points[x].x;
			c.segments[x].sy = points[x].y;
			c.segments[x].sz = points[x].z;
			
			c.segments[x].ex = points[x+1].x;
			c.segments[x].ey = points[x+1].y;
			c.segments[x].ez = points[x+1].z;
		}
		
		c.used = true;
	}
	else
	{
		c.used = false;
	}
	
	return c;
}

#define CAVE_MASK_X ((1 << (CHUNK_X_S + CAVE_BOX_S)) - 1)
#define CAVE_MASK_Y ((1 << (CHUNK_Y_S + CAVE_BOX_S)) - 1)
#define CAVE_MASK_Z ((1 << (CHUNK_Z_S + CAVE_BOX_S)) - 1)

CavePoint WorldGen::generate_cave_point(int64_t scx, int64_t scy, int64_t scz)
{
	CavePoint c;
	
	c.x = (scx << (CHUNK_X_S + CAVE_BOX_S)) | (pseudorand_var(3, scx, scy, scz) & CAVE_MASK_X);
	c.y = (scy << (CHUNK_Y_S + CAVE_BOX_S)) | (pseudorand_var(3, scy, scz, scx) & CAVE_MASK_Y);
	c.z = (scz << (CHUNK_Z_S + CAVE_BOX_S)) | (pseudorand_var(3, scz, scx, scy) & CAVE_MASK_Z);
	
	return c;
}

CaveSystem WorldGen::generate_local_caves(ChunkID const& idx)
{
	int64_t blockx = (uint64_t)(idx.x) >> CAVE_BOX_S;
	int64_t blocky = (uint64_t)(idx.y) >> CAVE_BOX_S;
	int64_t blockz = (uint64_t)(idx.z) >> CAVE_BOX_S;
	
	CaveSystem c;
	
	for(int z = -1; z <= 1; z++)
	{
		for(int y = -1; y <= 1; y++)
		{
			for(int x = -1; x <= 1; x++)
				c.points[z+1][y+1][x+1] = generate_cave_point(blockx + x, blocky + y, blockz + z);
		}
	}
	
	int index = 0;

	for(int k = 0; k <= 1; k++)
	{
		for(int j = -1; j <= 1; j++)
		{
			for(int i = -1; i <= 1; i++)
			{
				c.caves[index] = generate_cave(c.points[k][j+1][i+1], c.points[k+1][j+1][i+1]); index++;
				c.caves[index] = generate_cave(c.points[j+1][k][i+1], c.points[j+1][k+1][i+1]); index++;
				c.caves[index] = generate_cave(c.points[j+1][i+1][k], c.points[j+1][i+1][k+1]); index++;
			}
		}
	}
	
	return c;
}

void WorldGen::generate_chunk(ChunkID const& idx, Chunk* res)
{
	int64_t x, y, z;
	int64_t chunk_height = (idx.y << CHUNK_Y_S);
	
	SurfaceCell surface[CHUNK_Z + (SURFACE_GEN_PADDING << 1)][CHUNK_X + (SURFACE_GEN_PADDING << 1)];
	
	bool only_air_chunk = true;  //start off as true, and if we find a place where the surface is higher than the bottom of the chunk, set it to false.
	if(chunk_height <= WORLD_MAX_HEIGHT)  //early out for air chunks that are above the max height of the surface
	{
		if(chunk_height <= WATER_LEVEL)
			only_air_chunk = false;
		
		//generate the surface of the world
		y = (idx.z << CHUNK_Z_S) - SURFACE_GEN_PADDING;
		for(int64_t yp = 0; yp < CHUNK_Z + (SURFACE_GEN_PADDING << 1); yp++)
		{
			x = (idx.x << CHUNK_X_S) - SURFACE_GEN_PADDING;
			for(int64_t xp = 0; xp < CHUNK_X + (SURFACE_GEN_PADDING << 1); xp++)
			{
				surface[yp][xp] = generate_surface_data(x, y);
				
				if(surface[yp][xp].height >= chunk_height)
					only_air_chunk = false;
				
				x++;
			}
			y++;
		}
	}
	
	if(only_air_chunk)  //everything is air, no need to actually do any calculations, just push all 0s to the chunk.
	{
		void* ptr = (void*)(res->data);
		memset(ptr, 0, CHUNK_X*CHUNK_Y*CHUNK_Z*sizeof(Block));
		return;
	}
	
	//get the surface properties
	y = (idx.z << CHUNK_Z_S);
	for(int64_t yp = SURFACE_GEN_PADDING; yp < CHUNK_Z + SURFACE_GEN_PADDING; yp++)
	{
		x = (idx.x << CHUNK_X_S);
		for(int64_t xp = SURFACE_GEN_PADDING; xp < CHUNK_X + SURFACE_GEN_PADDING; xp++)
		{
			surface[yp][xp].nearWater = near_water(xp, yp, surface);
			x++;
		}
		y++;
	}
	
	CaveSystem cave = generate_local_caves(idx);
	
	//given the surface, generate each of the blocks in the chunk
	z = idx.z << CHUNK_Z_S;
	for(int64_t k=0; k<CHUNK_Z; k++)
	{
		y = idx.y << CHUNK_Y_S;
		for(int64_t j=0; j<CHUNK_Y; j++)
		{
			x = idx.x << CHUNK_X_S;
			for(int64_t i=0; i<CHUNK_X; i++)
			{
				auto b = generate_block(x, y, z, surface[k + SURFACE_GEN_PADDING][i + SURFACE_GEN_PADDING], cave);
			
				res->set(i, j, k, b);
				
				x++;
			}
			y++;
		}
		z++;
	}
	
}

};

