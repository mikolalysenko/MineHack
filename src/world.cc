#include <pthread.h>

#include <utility>
#include <cstring>
#include <cassert>
#include <utility>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

#include "constants.h"
#include "misc.h"
#include "chunk.h"
#include "entity.h"
#include "entity_db.h"
#include "world.h"

using namespace std;
using namespace Server;

namespace Game
{

//The world instance, handles game logic stuff
World::World()
{
	running = true;
	
	//Create config stuff
	config = new Config("data/mapconfig.tc");
	
	//Create map database
	world_gen = new WorldGen(config);
	game_map = new Map(world_gen, "data/map.tc");
	
	//Create entity database
	entity_db = new EntityDB("data/entities.tc", config);

	//Recover tick count
	tick_count = config->readInt("tick_count");
}

//Clean up/saving stuff
World::~World()
{
	//Save tick count
	config->storeInt(tick_count, "tick_count");

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
		uint64_t tick_count;
	
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
				
				//Set initial network data
				entity.player.net_last_tick = J->tick_count;
				entity.player.net_x = entity.base.x;
				entity.player.net_y = entity.base.y;
				entity.player.net_z = entity.base.z;
				entity.player.net_pitch = entity.base.pitch;
				entity.player.net_yaw	= entity.base.yaw;
				entity.player.net_roll	= entity.base.roll;
				entity.player.net_input = 0;
				
				J->success = true;
				return EntityUpdateControl::Update;
			}
		}
	};
	
	
	Join J = { false, tick_count };
	
	if(!entity_db->update_entity(player_id, Join::call, &J))
		return false;
		
	//Send a resync message to the player
	if(J.success)
		resync_player(player_id);
		
	//TODO: Push an initialize update to all players in region
		
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
	if(L.success)
		player_updates.clear_events(player_id);
		
	//TODO: Push destroy entity update to all players in region
	
	return L.success;
}


//Retrieves a compressed chunk from the server
int World::get_compressed_chunk(
	EntityID const& player_id,
	ChunkID const& chunk_id,
	uint8_t* buf,
	size_t buf_len)
{
	//TODO: Do sanity check on chunk request
	Entity entity;
	if( !entity_db->get_entity(player_id, entity) ||
		entity.base.type != EntityType::Player ||
		!entity.base.active )
	{
		return 0;
	}

	//Read the chunk from the map
	Chunk chunk;
	game_map->get_chunk(chunk_id, &chunk);
	
	//Initialize all entities in the chunk for the client
	get_entity_updates(player_id, Region(chunk_id), true);
	
	//Return the compressed chunk
	return chunk.compress((void*)buf, buf_len);
}
	
//Sends queued messages to client
void* World::heartbeat(
	EntityID const& player_id,
	int& len)
{
	//Retrieve player entity
	Entity player;
	if(!entity_db->get_entity(player_id, player))
	{
		len = 0;
		return NULL;
	}
	
	//Push a clock update to the player
	UpdateEvent update;
	update.type = UpdateEventType::SyncClock;
	update.clock_event.tick_count = tick_count;
	player_updates.send_event(player_id, update);
	
	//Get all entity updates in the player's region
	Region r( player.base.x - UPDATE_RADIUS, player.base.y - UPDATE_RADIUS, player.base.z - UPDATE_RADIUS,
			  player.base.x + UPDATE_RADIUS, player.base.y + UPDATE_RADIUS, player.base.z + UPDATE_RADIUS );
	get_entity_updates(player_id, r, false);

	//Push the events back to the player
	return player_updates.get_events(player_id, len);
}

//Sends a resynchronize packet to the player
void World::resync_player(EntityID const& player_id)
{
	UpdateEvent update;
	update.type = UpdateEventType::UpdateEntity;
	if(entity_db->get_entity(player_id, update.entity_event.entity))
	{
		player_updates.send_event(player_id, update);
	}
	
	update.type = UpdateEventType::SyncClock;
	update.clock_event.tick_count = tick_count;
	player_updates.send_event(player_id, update);
}

//Pushes entity updates to target player
void World::get_entity_updates(EntityID const& player_id, Region const& region, bool initialize)
{
	struct Visitor
	{
		EntityID player_id;
		UpdateEvent update;
		UpdateMailbox* player_updates;
	
		static EntityUpdateControl call(Entity& entity, void* data)
		{
			Visitor* v = (Visitor*)data;
			
			//TODO: Check if we need to replicate entity
			
			//Send update to player
			v->update.entity_event.entity = entity;
			v->player_updates->send_event(v->player_id, v->update);
			
			return EntityUpdateControl::Continue;
		}
	};


	Visitor V;
	V.player_id = player_id;
	V.update.type = UpdateEventType::UpdateEntity;
	V.update.entity_event.initialize = initialize;
	V.player_updates = &player_updates;
	
	entity_db->foreach(Visitor::call, &V);
}


//Broadcasts an event to all players
void World::broadcast_update(UpdateEvent const& update, Region const& r)
{
	struct Broadcast
	{
		const UpdateEvent*	update;
		UpdateMailbox*		mailbox;
		
		static EntityUpdateControl call(Entity& entity, void* data)
		{
			Broadcast* broadcast = (Broadcast*)data;
			broadcast->mailbox->send_event(entity.entity_id, *broadcast->update);
			return EntityUpdateControl::Continue;
		}
	};
	
	Broadcast B = { &update, &player_updates };
	entity_db->foreach(Broadcast::call, &B, r, { EntityType::Player }, true);
}

//Ticks the server
void World::tick()
{
	tick_count++;
	
	//The entity loop:  Updates all entities in the game
	struct Visitor
	{
		World*				world;
		vector<EntityID>	dead_players;
		
		static EntityUpdateControl call(Entity& entity, void* data)
		{
			Visitor *V = (Visitor*)data;
		
			if(entity.base.type == EntityType::Player)
			{
				//Check timeout condition
				if(V->world->tick_count - entity.player.net_last_tick > PLAYER_TIMEOUT)
				{
					//Log out player and post a delete event
					V->dead_players.push_back(entity.entity_id);					
					return EntityUpdateControl::Continue;
				}
			
				//Update network state (dumb, no interpolation, no using input)
				entity.base.x = entity.player.net_x;
				entity.base.y = entity.player.net_y;
				entity.base.z = entity.player.net_z;
				
				entity.base.pitch = entity.player.net_pitch;
				entity.base.yaw = entity.player.net_yaw;
				entity.base.roll = 0.0;
				
				return EntityUpdateControl::Update;				
			}
		
			return EntityUpdateControl::Continue;
		}
	};
	
	//Traverse all entities
	Visitor V;
	V.world = this;
	
	for(int i=0; i<V.dead_players.size(); i++)
	{
		player_leave(V.dead_players[i]);
	}
	
	entity_db->foreach(Visitor::call, &V); 
}


//Handles a client input event
void World::handle_input(
	EntityID const& player_id, 
	InputEvent const& input)
{
	switch(input.type)
	{
		case InputEventType::PlayerTick:
			handle_player_tick(player_id, input.player_event);
		break;
		
		case InputEventType::Chat:
			cout << "Got chat event: " << player_id.id << endl;
			handle_chat(player_id, input.chat_event);
		break;
	}
}

void World::handle_player_tick(EntityID const& player_id, PlayerEvent const& input)
{
	struct PlayerTick
	{
		const PlayerEvent* input;
		uint64_t tick_count;
		bool out_of_sync;
		
		static EntityUpdateControl call(Entity& entity, void* data)	
		{
			PlayerTick* player_tick 	= (PlayerTick*)data;
			PlayerEntity* player 		= &entity.player;
			const PlayerEvent* input	= player_tick->input;

			//Set player controls			
			player->net_input		= input->input_state;
			
			//Do a sanity check on the player position
			if(	abs(entity.base.x - input->x) > POSITION_RESYNC_RADIUS ||
				abs(entity.base.y - input->y) > POSITION_RESYNC_RADIUS ||
				abs(entity.base.z - input->z) > POSITION_RESYNC_RADIUS ||
				player_tick->tick_count < input->tick ||
				input->tick < player_tick->tick_count - TICK_RESYNC_TIME ||
				input->tick < player->net_last_tick )
			{
				player_tick->out_of_sync = true;
				return EntityUpdateControl::Continue;
			}
			
			//Upate player network state
			player->net_last_tick	= input->tick;
			player->net_x			= input->x;
			player->net_y			= input->y;
			player->net_z			= input->z;
			player->net_pitch		= input->pitch;
			player->net_yaw			= input->yaw;
			player->net_roll		= input->roll;
			
			return EntityUpdateControl::Update;
		}
	};
	
	
	PlayerTick T = { &input, tick_count, false };
	entity_db->update_entity(player_id, PlayerTick::call, &T);
	
	if(T.out_of_sync)
	{
		resync_player(player_id);
	}
}


void World::handle_chat(EntityID const& player_id, ChatEvent const& input)
{
	Entity entity;
	if( !entity_db->get_entity(player_id, entity) ||
		!entity.base.active ||
		entity.base.type != EntityType::Player )
		return;
	
	//Unpack chat event
	UpdateEvent update;
	update.type = UpdateEventType::Chat;
	
	update.chat_event.name_len = strlen(entity.player.player_name);
	memcpy(update.chat_event.name, entity.player.player_name, update.chat_event.name_len);
	update.chat_event.name[update.chat_event.name_len] = '\0';
	
	update.chat_event.msg_len = input.len;
	memcpy(update.chat_event.msg, input.msg, input.len);
	update.chat_event.msg[input.len] = '\0';
	
	//Construct region
	Region r(	entity.base.x - CHAT_RADIUS, entity.base.y - CHAT_RADIUS, entity.base.z - CHAT_RADIUS,  
				entity.base.x + CHAT_RADIUS, entity.base.y + CHAT_RADIUS, entity.base.z + CHAT_RADIUS); 
	
	//Push the update
	broadcast_update(update, r);
}


};
