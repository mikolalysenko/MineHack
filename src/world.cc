#include <pthread.h>

#include <utility>
#include <cstring>
#include <cassert>
#include <utility>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

#include "misc.h"
#include "chunk.h"
#include "world.h"

using namespace std;
using namespace Server;

namespace Game
{

//The world instance, handles game logic stuff
World::World()
{
	running = true;
	
	//Clear tick count
	tick_count = 0;
	
	//Create config stuff
	config = new Config("data/mapconfig.tc");
	
	//Create map database
	world_gen = new WorldGen(config);
	game_map = new Map(world_gen, "data/map.tc");
	
	//Create entity database
	entity_db = new EntityDB("data/entities.tc");
	
	//Create player database
	player_db = new PlayerDB("data/players.tc");
}

//Clean up/saving stuff
World::~World()
{
	delete player_db;
	delete entity_db;
	delete game_map;
	delete world_gen;
	delete config;
}

//Creates a player
bool World::player_create(string const& player_name)
{
	return player_db->create_player(player_name);
}

//Called to delete a player
void World::player_delete(string const& player_name)
{
	player_db->remove_player(player_name);
}

//Called when player joins game
bool World::player_join(string const& player_name)
{
	struct JoinAction
	{
		bool success;
	
		static PlayerUpdate update(PlayerState& player_state, void* data)
		{
			JoinAction* action = (JoinAction*)data;
			
			if(player_state.logged_in)
			{
				//Player already logged in
				return PlayerUpdate::None;
			}
			else
			{
				//TODO: Create entity for player
				
				//Set initial state
				player_state.logged_in		= true;
				player_state.input_state	= 0;
				
				//Set success
				action->success = true;
				
				//Return
				return PlayerUpdate::Update;
			}
		}
	};
	
	
	JoinAction action;
	action.success = false;
	
	if(!player_db->update_player(player_name, JoinAction::update, &action))
		return false;
		
	return action.success;
}


//Handle a player leave event
bool World::player_leave(std::string const& player_name)
{
	struct LeaveAction
	{
		bool success;
	
		static PlayerUpdate update(PlayerState& player_state, void* data)
		{
			LeaveAction* action = (LeaveAction*)data;
			
			if(player_state.logged_in)
			{
				//TODO: Remove player entity
				
				//Clear player state
				player_state.entity_id.clear();
				player_state.logged_in		= false;
				player_state.input_state	= 0;
				
				//Set success
				action->success = true;
				
				return PlayerUpdate::Update;
			}
			else
			{
				return PlayerUpdate::None;
			}
		}
	};
	
	
	LeaveAction action;
	action.success = false;
	
	if(!player_db->update_player(player_name, LeaveAction::update, &action))
		return false;
		
	return action.success;
}


//Handles a client input event
void World::handle_input(
	string const& player_name, 
	InputEvent const& ev)
{
	//TODO: Dispatch event here
}



//Retrieves a compressed chunk from the server
int World::get_compressed_chunk(
	string const& player_name,
	ChunkID const& chunk_id,
	uint8_t* buf,
	size_t buf_len)
{
	//TODO: Validate player coordinates


	//Read the chunk from the map
	Chunk chunk;
	game_map->get_chunk(chunk_id, &chunk);
	
	//Return the compressed chunk
	return chunk.compress((void*)buf, buf_len);
}
	
//Sends queued messages to client
void* World::heartbeat(
	string const& player_name,
	int& len)
{
	return player_updates.get_events(player_name, len);
}

//Broadcasts an event to all players
void World::broadcast_update(UpdateEvent const& update, Region const& r)
{
	//TODO: Implement this
}

//Ticks the server
void World::tick()
{
	//TODO: Implement this
}

};
