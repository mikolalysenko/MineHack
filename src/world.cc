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
	delete session_manager;
	delete entity_db;
}

//Creates a player
bool World::player_create(string const& player_name)
{
	return true;
}

//Called to delete a player
bool World::player_delete(string const& player_name)
{
	return true;
}

//Attempt to add player to the game
bool World::player_join(string const& player_name, SessionID& session_id)
{
	return session_manager->create_session(player_name, session_id);
}

//Handle a player leave event
bool World::player_leave(SessionID const& session_id)
{
	return true;
}

//Attach an update socket
bool World::player_attach_update_socket(SessionID const& session_id, WebSocket* socket)
{
	return session_manager->attach_update_socket(session_id, socket);
}

//Attach a map socket
bool World::player_attach_map_socket(SessionID const& session_id, WebSocket* socket)
{
	return session_manager->attach_map_socket(session_id, socket);
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
