#include <string>

#include <pthread.h>

#include <cstring>
#include <cstdlib>
#include <cstdint>

#include <tcutil.h>
#include <tchdb.h>

#include "misc.h"
#include "map.h"

using namespace std;


namespace Game
{

Map::Map(WorldGen* gen, string const& filename) : world_gen(gen)
{
	//Initialize the map database
	map_db = tchdbnew();
	
	//Set options
	tchdbsetmutex(map_db);
	tchdbtune(map_db,
		0,							//Number of buckets
		16,							//Record alignment
		10,							//Free elements
		HDBTLARGE | HDBTBZIP);
	tchdbsetcache(map_db,
		1024);
	
	//Open the map database
	tchdbopen(map_db, filename.c_str(), HDBOWRITER | HDBOCREAT);
}

Map::~Map()
{
	tchdbclose(map_db);
	tchdbdel(map_db);
}

//Retrieves a particular chunk from the map
void Map::get_chunk(ChunkID const& idx, Chunk* chunk)
{
	int l = tchdbget3(map_db, 
		(const void*)&idx, sizeof(ChunkID),
		(void*)chunk, sizeof(Chunk));
	

	//If the value of l was not set correctly, need to regenerate the chunk
	if(l != sizeof(Chunk))
	{
		//Generate the chunk
		world_gen->generate_chunk(idx, chunk);
		
		//Store chunk in database
		if(!tchdbputkeep(map_db,
			(const void*)&idx, sizeof(ChunkID),
			(const void*)chunk, sizeof(Chunk)))
		{
			tchdbget3(map_db, 
				(const void*)&idx, sizeof(ChunkID),
				(void*)chunk, sizeof(Chunk));
		}
	}
}

//Retrieves a particular chunk from the map
void Map::get_surface_chunk(ChunkID const& c, Chunk* chunk)
{
	Chunk center, left, right, top, bottom, front, back;
	
	get_chunk(c, &center);
	get_chunk(ChunkID(c.x-1, c.y, c.z), &left);
	get_chunk(ChunkID(c.x+1, c.y, c.z), &right);
	get_chunk(ChunkID(c.x, c.y-1, c.z), &bottom);
	get_chunk(ChunkID(c.x, c.y+1, c.z), &top);
	get_chunk(ChunkID(c.x, c.y, c.z-1), &front);
	get_chunk(ChunkID(c.x, c.y, c.z+1), &back);
	
	for(int iz=0; iz<CHUNK_Z; ++iz)
	for(int iy=0; iy<CHUNK_Y; ++iy)
	for(int ix=0; ix<CHUNK_X; ++ix)
	{
		int idx = ix + (iy<<CHUNK_X_S) + (iz<<CHUNK_XY_S);
		
		//Read in current cell
		Block b = center.data[idx], ob;
		
		if(BLOCK_TRANSPARENCY[(int)b])
		{
			chunk->data[idx] = b;
			continue;
		}
	
		//Check for surface block
		if(ix > 0)
		{
			ob = center.data[idx - 1];
		}
		else
		{
			ob = left.data[CHUNK_X - 1 + (iy << CHUNK_X_S) + (iz << CHUNK_XY_S)];
		}
		if(BLOCK_TRANSPARENCY[(int)ob])
		{
			chunk->data[idx] = b;
			continue;
		}
				
		if(ix < CHUNK_X - 1)
		{
			ob = center.data[idx + 1];
		}
		else
		{
			ob = right.data[(iy << CHUNK_X_S) + (iz << CHUNK_XY_S)];
		}
		if(BLOCK_TRANSPARENCY[(int)ob])
		{
			chunk->data[idx] = b;
			continue;
		}
		
		if(iy > 0)
		{
			ob = center.data[idx - CHUNK_X];
		}
		else
		{
			ob = bottom.data[ix + ((CHUNK_Y-1) << CHUNK_X_S) + (iz << CHUNK_XY_S)];
		}
		if(BLOCK_TRANSPARENCY[(int)ob])
		{
			chunk->data[idx] = b;
			continue;
		}
		
		if(iy < CHUNK_Y - 1)
		{
			ob = center.data[idx + CHUNK_X];
		}
		else
		{
			ob = top.data[ix + (iz << CHUNK_XY_S)];
		}
		if(BLOCK_TRANSPARENCY[(int)ob])
		{
			chunk->data[idx] = b;
			continue;
		}
		
		if(iz > 0)
		{
			ob = center.data[idx - CHUNK_X*CHUNK_Y];
		}
		else
		{
			ob = front.data[ix + (iy << CHUNK_X_S) + ((CHUNK_Z-1) << CHUNK_XY_S)];
		}
		if(BLOCK_TRANSPARENCY[(int)ob])
		{
			chunk->data[idx] = b;
			continue;
		}
				
		if(iz < CHUNK_Z - 1)
		{
			ob = center.data[idx + CHUNK_X*CHUNK_Y];
		}
		else
		{
			ob = back.data[ix + (iy << CHUNK_X_S)];
		}
		if(BLOCK_TRANSPARENCY[(int)ob])
		{
			chunk->data[idx] = b;
			continue;
		}
		
		chunk->data[idx] = Block::Nonsense;
	}
}


//Sets a block in the map
void Map::set_block(int x, int y, int z, Block b)
{
	auto idx = COORD2CHUNKID(x, y, z);
	Chunk c;

	while(true)
	{
		if(!tchdbtranbegin(map_db))
			return;
	
		int l = tchdbget3(map_db, 
			(const void*)&idx, sizeof(ChunkID),
			(void*)&c, sizeof(Chunk));
		
		if(l != sizeof(Chunk))
		{
			tchdbtranabort(map_db);
			return;
		}

		c.data[CHUNK_OFFSET(x & CHUNK_X_MASK, 
							y & CHUNK_Y_MASK, 
							z & CHUNK_Z_MASK)] = b;
						
		tchdbput(map_db,
			(const void*)&idx, sizeof(ChunkID),
			(void*)&c, sizeof(Chunk));
		
		if(tchdbtrancommit(map_db))
			return;
	}
}

//Retrieves a chunk from the map
Block Map::get_block(int x, int y, int z)
{
	Chunk c;	
	get_chunk(COORD2CHUNKID(x,y,z), &c);
	
	return c.data[CHUNK_OFFSET(x & CHUNK_X_MASK, 
								y & CHUNK_Y_MASK, 
								z & CHUNK_Z_MASK)];
}

};
