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
		0,		//Number of buckets
		15,		//Record alignment
		10,		//Free elements
		HDBTLARGE | HDBTBZIP);
	tchdbsetcache(map_db,
		1024);
	
	//Open the map database
	tchdbopen(map_db, filename.c_str(), HDBOWRITER | HDBOCREAT);
}

void Map::shutdown()
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

void Map::set_block(int x, int y, int z, Block b)
{
	ChunkID idx(x>>5, y>>5, z>>5);
	Chunk c;

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

	c.data[(x&31) + ((y&31)<<5) + ((z&31)<<10)] = b;
	
	tchdbput(map_db,
		(const void*)&idx, sizeof(ChunkID),
		(void*)&c, sizeof(Chunk));
		
	tchdbtrancommit(map_db);
}

//Retrieves a chunk from the map
Block Map::get_block(int x, int y, int z)
{
	Chunk c;	
	get_chunk(ChunkID(x>>5, y>>5, z>>5), &c);
	return c.data[(x&31) + ((y&31)<<5) + ((z&31)<<10)];
}



};
