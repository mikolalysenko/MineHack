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

Block WorldGen::generate_block(int64_t x, int64_t y, int64_t z, SurfaceCell surface, CaveSystem cave)
{
	//early out for stuff over the surface, not needed for correct execution
	if(y > surface.height && y > WATER_LEVEL)
		return Block::Air;
	
	if(y <= surface.height && cave.check_dist(x, y, z, 16))  //don't need to check caves in the water or air
		return Block::Air;
	
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
	
	CaveSystem cave(idx);
	
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

