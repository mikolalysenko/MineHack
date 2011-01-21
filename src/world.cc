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
#include "input_event.h"
#include "entity_db.h"
#include "mailbox.h"
#include "world.h"

using namespace std;
using namespace Game;

namespace Game
{


//---------------------------------------------------------------
// Constructors
//---------------------------------------------------------------

//The world instance, handles game logic stuff
World::World()
{
	running = true;
	
	config = new Config("data/mapconfig.tc");
	world_gen = new WorldGen(config);
	game_map = new Map(world_gen, "data/map.tc");
	entity_db = new EntityDB("data/entities.tc", config);
	mailbox = new Mailbox();

	//Restore tick count
	tick_count = config->readInt("tick_count");
}

//Clean up/saving stuff
World::~World()
{
	//Save tick count
	config->storeInt(tick_count, "tick_count");

	delete mailbox;
	delete entity_db;
	delete game_map;
	delete world_gen;
	delete config;
}


//---------------------------------------------------------------
// Player management
//---------------------------------------------------------------

//Creates a player
bool World::player_create(string const& player_name, EntityID& player_id)
{
	if(player_name.size() > PLAYER_NAME_MAX_LEN)
		return false;

	//Initialize player entity
	Entity player;
	player.base.type	= EntityType::Player;
	player.base.active	= false;
	player.base.x		= PLAYER_START_X;
	player.base.y		= PLAYER_START_Y;
	player.base.z		= PLAYER_START_Z;
	player.base.pitch	= 0;
	player.base.yaw		= 0;
	player.base.roll	= 0;
	
	//Set initial network data
	player.player.net_last_tick = tick_count;
	player.player.net_x 		= player.base.x;
	player.player.net_y 		= player.base.y;
	player.player.net_z 		= player.base.z;
	player.player.net_pitch 	= player.base.pitch;
	player.player.net_yaw		= player.base.yaw;
	player.player.net_roll		= player.base.roll;
	player.player.net_input 	= 0;
	
	//Set player name
	assert(player_name.size() <= PLAYER_NAME_MAX_LEN);
	memset(player.player.player_name, 0, PLAYER_NAME_MAX_LEN + 1);
	memcpy(player.player.player_name, player_name.data(), player_name.size());
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
	struct Visitor
	{
		bool success;
		uint64_t tick_count;
		double x, y, z;
	
		static EntityUpdateControl call(Entity& entity, void* data)
		{
			Visitor* V = (Visitor*)data;
			
			if( entity.base.type != EntityType::Player ||
				entity.base.active )
			{	
				return EntityUpdateControl::Continue;
			}
			else
			{
				//Activate player
				entity.base.active = true;
				
				//Set initial network data
				entity.player.net_last_tick = V->tick_count;
				entity.player.net_x = entity.base.x;
				entity.player.net_y = entity.base.y;
				entity.player.net_z = entity.base.z;
				entity.player.net_pitch = entity.base.pitch;
				entity.player.net_yaw	= entity.base.yaw;
				entity.player.net_roll	= entity.base.roll;
				entity.player.net_input = 0;

				//Set coordinates
				V->x = entity.base.x;
				V->y = entity.base.y;
				V->z = entity.base.z;
				
				//Set success code			
				V->success = true;
				return EntityUpdateControl::Update;
			}
		}
	};
	
	//Initialize player
	Visitor V = { false, tick_count, 0., 0., 0. };
	if(!entity_db->update_entity(player_id, Visitor::call, &V) || !V.success)
		return false;
	
	//Register player mail box
	mailbox->add_player(player_id);
	mailbox->set_origin(player_id, (int)V.x, (int)V.y, (int)V.z);
		
	//Resynchronize player position
	resync_player(player_id);
	
	return true;
}


//Handle a player leave event
bool World::player_leave(EntityID const& player_id)
{
	struct Visitor
	{
		bool success;
		double x, y, z;
	
		static EntityUpdateControl call(Entity& entity, void* data)
		{
			Visitor* V = (Visitor*)data;
			
			if( entity.base.type != EntityType::Player ||
				!entity.base.active )
			{	
				return EntityUpdateControl::Continue;
			}
			else
			{
				//Deactivate player
				entity.base.active = false;
				
				//Set return value
				V->success = true;
				V->x = entity.base.x;
				V->y = entity.base.y;
				V->z = entity.base.z;
				
				return EntityUpdateControl::Update;
			}
		}
	};
	
	//Destroy player's mailbox
	mailbox->del_player(player_id);

	//Log player out
	Visitor V = { false };
	if(!entity_db->update_entity(player_id, Visitor::call, &V))
		return false;
	
	//Post kill event to all players in range
	if(V.success)
	{
		Region r(
			V.x - UPDATE_RADIUS, V.y - UPDATE_RADIUS, V.z - UPDATE_RADIUS,
			V.x + UPDATE_RADIUS, V.y + UPDATE_RADIUS, V.z + UPDATE_RADIUS);
	
		struct KillVisitor
		{
			EntityID	casualty_id;
			Mailbox*	mailbox;
			
			static EntityUpdateControl call(Entity& entity, void* data)
			{
				KillVisitor* KV = (KillVisitor*)data;
				KV->mailbox->send_kill(entity.entity_id, KV->casualty_id);
				return EntityUpdateControl::Continue;
			}
		};
		
		KillVisitor KV = { player_id, mailbox };
		entity_db->foreach(KillVisitor::call, &KV, r, { EntityType::Player });
	}

	
	return V.success;
}


//---------------------------------------------------------------
// Input handlers
//---------------------------------------------------------------

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
			handle_chat(player_id, input.chat_event);
		break;
	}
}

void World::handle_player_tick(EntityID const& player_id, PlayerEvent const& input)
{
	struct Visitor
	{
		const PlayerEvent* input;
		uint64_t tick_count;
		bool out_of_sync;
		
		static EntityUpdateControl call(Entity& entity, void* data)	
		{
			Visitor* V					= (Visitor*)data;
			PlayerEntity* player 		= &entity.player;
			const PlayerEvent* input	= V->input;

			//Set player controls			
			player->net_input		= input->input_state;
			
			//Do a sanity check on the player position
			if(	abs(entity.base.x - input->x) > POSITION_RESYNC_RADIUS ||
				abs(entity.base.y - input->y) > POSITION_RESYNC_RADIUS ||
				abs(entity.base.z - input->z) > POSITION_RESYNC_RADIUS ||
				V->tick_count < input->tick ||
				input->tick < V->tick_count - TICK_RESYNC_TIME ||
				input->tick < player->net_last_tick )
			{
				V->out_of_sync = true;
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
	
	
	Visitor V = { &input, tick_count, false };
	entity_db->update_entity(player_id, Visitor::call, &V);
	
	if(V.out_of_sync)
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
	
	//Construct chat string
	stringstream ss;
	ss << entity.player.player_name << ": " << input.msg;
	
	//Construct region
	Region r(	entity.base.x - CHAT_RADIUS, entity.base.y - CHAT_RADIUS, entity.base.z - CHAT_RADIUS,  
				entity.base.x + CHAT_RADIUS, entity.base.y + CHAT_RADIUS, entity.base.z + CHAT_RADIUS); 
	
	
	//Post event to all players in radius
	struct Visitor
	{
		Mailbox* mailbox;
		string chat_string;
	
		static EntityUpdateControl call(Entity& p, void* data)
		{
			Visitor* V = (Visitor*)data;
			V->mailbox->send_chat(p.entity_id, V->chat_string);
			return EntityUpdateControl::Continue;
		}
	};
	
	Visitor V = { mailbox, ss.str() };
	entity_db->foreach(Visitor::call, &V, r, { EntityType::Player } );
}


//---------------------------------------------------------------
// Chunk retrieval
//---------------------------------------------------------------

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
		entity.base.type != EntityType::Player ||
		!entity.base.active || 
		abs(entity.base.x - (chunk_id.x + .5) * CHUNK_X) >= UPDATE_RADIUS ||
		abs(entity.base.y - (chunk_id.y + .5) * CHUNK_Y) >= UPDATE_RADIUS ||
		abs(entity.base.z - (chunk_id.z + .5) * CHUNK_Z) >= UPDATE_RADIUS )
	{
		return 0;
	}

	//Read the chunk from the map
	Chunk chunk;
	game_map->get_chunk(chunk_id, &chunk);
	
	//Return the compressed chunk
	return chunk.compress((void*)buf, buf_len);
}


//---------------------------------------------------------------
// Heartbeat
//---------------------------------------------------------------

//Sends queued messages to client
void World::heartbeat(
	EntityID const& player_id,
	int socket)
{
	//Retrieve player entity
	Entity player;
	if(!entity_db->get_entity(player_id, player))
	{
		return;
	}

	//Create range
	Region r( 
		player.base.x - UPDATE_RADIUS, player.base.y - UPDATE_RADIUS, player.base.z - UPDATE_RADIUS,
		player.base.x + UPDATE_RADIUS, player.base.y + UPDATE_RADIUS, player.base.z + UPDATE_RADIUS);

	//Start bulk update
	//When destructor fires, will send the http event to the client
	BulkUpdater bu(
		mailbox,
		player_id,
		socket,
		tick_count,
		player.base.x, player.base.y, player.base.z);

	//Traverse all entities which need to be updated
	entity_db->foreach(BulkUpdater::send_entity, bu.get_data(), r);
}

//Sends a resynchronize packet to the player
void World::resync_player(EntityID const& player_id)
{
	Entity player;
	if(entity_db->get_entity(player_id, player))
	{
		mailbox->send_entity(player_id, player);
	}
}

//---------------------------------------------------------------
// World ticking / update
//---------------------------------------------------------------

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

};
