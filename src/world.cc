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
}

//Clean up/saving stuff
World::~World()
{
	delete entity_db;
	delete game_map;
	delete world_gen;
	delete config;
}

//Creates a player
bool World::player_create(string const& player_name, EntityID& player_id)
{
	if(player_name.size() > PLAYER_NAME_MAX_LEN)
		return false;

	Entity player;
	player.base.type	= EntityType::Player;
	player.base.active	= false;
	player.base.x		= PLAYER_START_X;
	player.base.y		= PLAYER_START_Y;
	player.base.z		= PLAYER_START_Z;
	player.base.pitch	= 0;
	player.base.yaw		= 0;
	player.base.roll	= 0;
	player.base.health	= 100;
	
	memcpy(player.player.player_name, player_name.c_str(), player_name.size());
	player.player.player_name[player_name.size()] = 0;
	
	return entity_db->create_entity(player, player_id);
}

//Retrieves a player entity
bool World::get_player_entity(string const& player_name, EntityID& player_id)
{
	return entity_db->get_player(player_name, player_id);
}

//Called to delete a player
void World::player_delete(EntityID const& player_id)
{
	entity_db->destroy_entity(player_id);
}

//Attempt to add player to the game
bool World::player_join(EntityID const& player_id)
{
	struct Join
	{
		bool success;
	
		static EntityUpdateControl call(Entity& entity, void* data)
		{
			Join* J = (Join*)data;
			
			if( entity.base.type != EntityType::Player ||
				entity.base.active )
			{	
				return EntityUpdateControl::Continue;
			}
			else
			{
				entity.base.active = true;
				J->success = true;
				return EntityUpdateControl::Update;
			}
		}
	};
	
	
	Join J = { false };
	
	if(!entity_db->update_entity(player_id, Join::call, &J))
		return false;
		
	return J.success;
}


//Handle a player leave event
bool World::player_leave(EntityID const& player_id)
{
	struct Leave
	{
		bool success;
	
		static EntityUpdateControl call(Entity& entity, void* data)
		{
			Leave* L = (Leave*)data;
			
			if( entity.base.type != EntityType::Player ||
				!entity.base.active )
			{	
				return EntityUpdateControl::Continue;
			}
			else
			{
				entity.base.active = false;
				L->success = true;
				return EntityUpdateControl::Update;
			}
		}
	};
	
	
	Leave L = { false };
	
	if(!entity_db->update_entity(player_id, Leave::call, &L))
		return false;
	return L.success;
}


//Handles a client input event
void World::handle_input(
	EntityID const& player_id, 
	InputEvent const& input)
{
}



//Retrieves a compressed chunk from the server
int World::get_compressed_chunk(
	EntityID const& player_id,
	ChunkID const& chunk_id,
	uint8_t* buf,
	size_t buf_len)
{
	//Do sanity check on chunk request
	Entity entity;
	if( !entity_db->get_entity(player_id, entity) ||
		entity.base.type != EntityType::Player )
	{
		return 0;
	}

	//Read the chunk from the map
	Chunk chunk;
	game_map->get_chunk(chunk_id, &chunk);
	
	//Return the compressed chunk
	return chunk.compress((void*)buf, buf_len);
}
	
//Sends queued messages to client
void* World::heartbeat(
	EntityID const& player_id,
	int& len)
{
	return player_updates.get_events(player_id, len);
}

//Broadcasts an event to all players
void World::broadcast_update(UpdateEvent const& update, Region const& r)
{
	//TODO: Implement
}

//Ticks the server
void World::tick()
{
	//TODO: Implement this
}

};
