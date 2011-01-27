#ifndef LIGHT_H
#define LIGHT_H

#include <string>
#include <cstdint>
#include <cstdlib>

#include <pthread.h>

#include <tcutil.h>
#include <tchdb.h>

#include "chunk.h"
#include "map.h"

namespace Game
{
	struct LightCalculator
	{
		Map* game_map;
		
		void compute_light(ChunkID const& lo, ChunkID const& hi, int niter=30);
	};
};


#endif

