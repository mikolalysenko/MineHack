#include "chunk.h"
#include "world.h"
#include "noise.h"

using namespace std;


//level at which water is
#define WATER_LEVEL				(1<<20)

//height of the tallest possible mountain
#define WORLD_MAX_HEIGHT		(WATER_LEVEL+30)

//height of the lowest possible point of the ocean
#define WORLD_MIN_HEIGHT		(WATER_LEVEL-30)

//scalar for the noise function
#define NOISE_SCALAR    		256

//how fine grained the features on land are (higher = more fine grained, but slower performance)
#define OCTAVES                 4

//padding around the current block to generate surface data
#define SURFACE_GEN_PADDING     3

namespace Game
{

Block WorldGen::generate_block(int x, int y, int z, SurfaceCell surface)
{
	if(y == surface.height && y > WATER_LEVEL)
		return Block::Grass;
	
	if(y <= surface.height && y + 2 > WATER_LEVEL)
		return Block::Sand;

	if(y < surface.height)
		return Block::Stone;
	
	if(y <= WATER_LEVEL)
		return Block::Water;
	
	return Block::Air;
}

SurfaceCell WorldGen::generate_surface_data(int cx, int cy)
{
	SurfaceCell cell;
	
	double fxp = ((double)cx) / NOISE_SCALAR;
	double fyp = ((double)cy) / NOISE_SCALAR;
	
	cell.height = (simplexNoise2D(fxp, fyp, OCTAVES) * (WORLD_MAX_HEIGHT - WORLD_MIN_HEIGHT)) + WORLD_MIN_HEIGHT;
	
	return cell;
}

void WorldGen::generate_chunk(ChunkID const& idx, Chunk* res)
{
	SurfaceCell surface[CHUNK_Z + (SURFACE_GEN_PADDING << 1)][CHUNK_X + (SURFACE_GEN_PADDING << 1)];
	
	//generate the surface of the world
	int y = (idx.z << CHUNK_Z_S) - SURFACE_GEN_PADDING;
	for(int yp = 0; yp < CHUNK_Z + (SURFACE_GEN_PADDING << 1); yp++)
	{
		int x = (idx.x << CHUNK_X_S) - SURFACE_GEN_PADDING;
		for(int xp = 0; xp < CHUNK_X + (SURFACE_GEN_PADDING << 1); xp++)
		{
			surface[yp][xp] = generate_surface_data(x, y);
			x++;
		}
		y++;
	}
	
	//get the surface properties
	y = (idx.z << CHUNK_Z_S);
	for(int yp = 0; yp < CHUNK_Z; yp++)
	{
		int x = (idx.x << CHUNK_X_S);
		for(int xp = 0; xp < CHUNK_X; xp++)
		{
			x++;
		}
		y++;
	}
	
	//given the surface, generate each of the blocks in the chunk
	int z = idx.z << CHUNK_Z_S;
	for(int k=0; k<CHUNK_Z; k++)
	{
		y = idx.y << CHUNK_Y_S;
		for(int j=0; j<CHUNK_Y; j++)
		{
			int x = idx.x << CHUNK_X_S;
			for(int i=0; i<CHUNK_X; i++)
			{
				res->set(i, j, k, generate_block(x, y, z, surface[k + SURFACE_GEN_PADDING][i + SURFACE_GEN_PADDING]));
				x++;
			}
			y++;
		}
		z++;
	}
}

};

