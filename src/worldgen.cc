#include "chunk.h"
#include "world.h"

using namespace std;


#define SURFACE_LEVEL	(1<<20)


namespace Game
{

Block WorldGen::generate_block(int x, int y, int z)
{
	if(y < SURFACE_LEVEL)
	{
		return Block::Stone;
	}
	return Block::Air;
}

Chunk* WorldGen::generate_chunk(const ChunkId& idx)
{
	auto res = new Chunk();
	
	for(int i=0; i<CHUNK_X; i++)
	for(int j=0; j<CHUNK_Y; j++)
	for(int k=0; k<CHUNK_Z; k++)
	{
		int x = idx.x * CHUNK_X + i,
			y = idx.y * CHUNK_Y + j,
			z = idx.z * CHUNK_Z + k;
		
		res->data[i][j][k] = generate_block(x, y, z);
	}
	
	for(int i=0; i<256; i++)
	{
		res->data[0][0][i] = (Block)i;
	}
	
	return res;
}

};

