#include <string>
#include <cstdio>

#include <stdint.h>

#include <tbb/task.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include "constants.h"
#include "misc.h"
#include "config.h"
#include "session.h"
#include "world.h"

using namespace tbb;
using namespace std;
using namespace Game;

//Uncomment this line to get dense logging for the web server
#define WORLD_DEBUG 1

#ifndef WORLD_DEBUG
#define DEBUG_PRINTF(...)
#else
#define DEBUG_PRINTF(...)  fprintf(stderr,__VA_ARGS__)
#endif


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
	//The world task
	struct WorldTask : task
	{
		World* instance;
		
		WorldTask(World* inst) : instance(inst) {}
	
		task* execute()
		{
			instance->main_loop();
			return NULL;
		}
	};
	
	running = true;
	world_task = new(task::allocate_root()) WorldTask(this);
	world_task->set_ref_count(1);
    task::spawn(*world_task); 

	return true;
}

//Stops the world
void World::stop()
{
	running = false;
	world_task->wait_for_all();
}

//Synchronize world state
void World::sync()
{
}

void World::main_loop()
{
	while(running)
	{
		//Iterate over the player set
		parallel_for(session_manager->sessions.range(), 
			[](concurrent_unordered_map<SessionID, Session*>::range_type sessions)
		{
			auto session = sessions.begin()->second;
		
			DEBUG_PRINTF("Processing session, %ld\n", session->session_id);
		});
		
		//Process all pending deletes
		session_manager->process_pending_deletes();
	}
}


};
