#include <string>

#include <pthread.h>

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
	//Initialize the world gen mutex
	pthread_mutex_init(&world_gen_lock, NULL);
	
	//Initialize the map database
	map_db = tchdbnew();
	
	//Set options
	tchdbsetmutex(map_db);
	tchdbtune(map_db,
		0,										//Number of buckets
		CHUNK_X_S + CHUNK_Y_S + CHUNK_Z_S,		//Record alignment (size of a chunk)
		10,										//Free elements
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
		MutexLock wgl(&world_gen_lock);
	
		//Verify that the chunk has not been regenerated
		l = tchdbget3(map_db, 
			(const void*)&idx, sizeof(ChunkID),
			(void*)chunk, sizeof(Chunk));
		if(l == sizeof(Chunk))
			return;
		
		//Generate the chunk
		world_gen->generate_chunk(idx, chunk);
		
		//Store chunk in database
		tchdbput(map_db,
			(const void*)&idx, sizeof(ChunkID),
			(const void*)chunk, sizeof(Chunk));
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
