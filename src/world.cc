#include "world.h"

namespace Game
{

World::World()
{
	gen = new WorldGen();
	game_map = new Map(gen);
}

};
