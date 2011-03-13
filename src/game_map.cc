#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <map>
#include <string>

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

Map::Map(WorldGen* gen, Config* cfg) : world_gen(gen), config(cfg)
{
}

Map::~Map()
{
	//FIXME: Release all old chunk buffers
}


//Retrieves a chunk
void GameMap::get_chunk(
	ChunkID const&, 
	Block* buffer, 
	int stride_x = CHUNK_X, 
	int stride_xy = CHUNK_X * CHUNK_Y)
{
}

//Retrieves a chunk
void GameMap::get_chunk_raw(
	ChunkID const&, 
	uint8_t* buffer,
	int* length)
{
}


};
