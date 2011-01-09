#ifndef GAME_H
#define GAME_H

#include "chunk.h"
#include "worldgen.h"
#include "map.h"

namespace Game
{

	struct World
	{
		WorldGen	*gen;
		Map    		*game_map;
		
		World();
	};
};

#endif

