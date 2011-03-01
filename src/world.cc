#include <string>
#include <stdint.h>

#include "constants.h"
#include "misc.h"
#include "config.h"
#include "session.h"
#include "world.h"

using namespace std;
using namespace Game;

namespace Game
{


//The world instance, handles game logic stuff
World::World(Config* cfg) : config(cfg)
{
	//Restore tick count
	ticks = config->readInt("tick_count");
	
	session_manager = new SessionManager();
	entity_db = new EntityDB(config);
}

//Clean up/saving stuff
World::~World()
{
	//Save tick count
	config->storeInt("tick_count", ticks);
	
	delete session_manager;
	delete entity_db;
}



//Creates a player
bool World::player_create(string const& player_name)
{
	return false;
}

//Called to delete a player
void World::player_delete(string const& player_name)
{
}

//Attempt to add player to the game
bool World::player_join(string const& player_id, SessionID const& session_id)
{
	return false;
}


//Handle a player leave event
void World::player_leave(SessionID const& session_id)
{
}

//Starts the world thread
bool World::start()
{
	return true;
}

//Stops the world
void World::stop()
{
}

//Synchronize world state
void World::sync()
{
}

};
