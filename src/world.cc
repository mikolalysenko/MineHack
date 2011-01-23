#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#include <utility>
#include <cstring>
#include <cassert>
#include <utility>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>

#include <tcutil.h>
#include <tctdb.h>

#include "constants.h"
#include "misc.h"
#include "chunk.h"
#include "entity.h"
#include "entity_db.h"
#include "mailbox.h"
#include "world.h"
#include "heartbeat.h"

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
	
	//Initialize subsystems
	config = new Config("data/mapconfig.tc");
	world_gen = new WorldGen(config);
	game_map = new Map(world_gen, "data/map.tc");
	entity_db = new EntityDB("data/entities.tc", config);
	mailbox = new Mailbox();
	
	//Set event handlers
	create_handler = new CreateHandler(this);
	update_handler = new UpdateHandler(this);
	delete_handler = new DeleteHandler(this);
	
	entity_db->set_create_handler(create_handler);
	entity_db->set_update_handler(update_handler);
	entity_db->set_delete_handler(delete_handler);

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
	
	delete create_handler;
	delete update_handler;
	delete delete_handler;
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
	player.base.flags	= EntityFlags::Inactive;
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
	cout << "DESTROYING PLAYER: " << player_id.id << endl;
	entity_db->destroy_entity(player_id);
}

//Attempt to add player to the game
bool World::player_join(EntityID const& player_id)
{
	struct Visitor
	{
		uint64_t tick_count;
		bool success;
		Entity player;
	
		static EntityUpdateControl call(Entity& entity, void* data)
		{
			Visitor* V = (Visitor*)data;
			if( !(entity.base.flags & EntityFlags::Inactive) )
			{	
				return EntityUpdateControl::Continue;
			}
			
			//Set flags
			entity.base.flags = EntityFlags::Poll | EntityFlags::Persist;
			
			//Set initial network data
			entity.player.net_last_tick = V->tick_count;
			entity.player.net_x = entity.base.x;
			entity.player.net_y = entity.base.y;
			entity.player.net_z = entity.base.z;
			entity.player.net_pitch = entity.base.pitch;
			entity.player.net_yaw	= entity.base.yaw;
			entity.player.net_roll	= entity.base.roll;
			entity.player.net_input = 0;

			//Set return values
			V->player = entity;
			V->success = true;
			
			return EntityUpdateControl::Update;
		}
	};
	
	//Initialize player
	Visitor V;
	V.success		= false;
	V.tick_count	= tick_count;
	if(!entity_db->update_entity(player_id, Visitor::call, &V) || !V.success)
		return false;
	
	//Register player mail box
	mailbox->add_player(player_id, 
		(int)V.player.base.x, 
		(int)V.player.base.y, 
		(int)V.player.base.z);
		
	return true;
}


//Handle a player leave event
bool World::player_leave(EntityID const& player_id)
{
	struct Visitor
	{
		double x, y, z;
	
		static EntityUpdateControl visit(Entity& entity, void* data)
		{
			if( entity.base.flags & EntityFlags::Inactive )
				return EntityUpdateControl::Continue;
			
			Visitor *V = (Visitor*)data;
			
			entity.base.flags = EntityFlags::Inactive;
			
			V->x = entity.base.x;
			V->y = entity.base.y;
			V->z = entity.base.z;
			
			return EntityUpdateControl::Update;
		}
	};
	
	mailbox->del_player(player_id);
	
	Visitor V;
	if(!entity_db->update_entity(player_id, Visitor::visit, &V))
		return false;
	
	return true;
}


//---------------------------------------------------------------
// Input handlers
//---------------------------------------------------------------

void World::handle_player_tick(EntityID const& player_id, PlayerEvent const& input)
{
	cout << "Updating player!" << endl;
	struct Visitor
	{
		const PlayerEvent* input;
		uint64_t tick_count;
		
		static EntityUpdateControl call(Entity& entity, void* data)	
		{
			//Sanity check player object
			if( (entity.base.flags & EntityFlags::Inactive) ||
				 entity.base.type != EntityType::Player )
				return EntityUpdateControl::Continue;
		
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
	
	Visitor V = { &input, tick_count };
	entity_db->update_entity(player_id, Visitor::call, &V);
}


void World::handle_chat(EntityID const& player_id, std::string const& msg)
{
	//Sanity check chat message
	Entity entity;
	if( !entity_db->get_entity(player_id, entity) ||
		(entity.base.flags & EntityFlags::Inactive) ||
		entity.base.type != EntityType::Player )
		return;
	
	//Construct chat string
	stringstream ss;
	ss << entity.player.player_name << ": " << msg;
	
	//Construct region
	Region r(	entity.base.x - CHAT_RADIUS,
				entity.base.y - CHAT_RADIUS,
				entity.base.z - CHAT_RADIUS,
				entity.base.x + CHAT_RADIUS,
				entity.base.y + CHAT_RADIUS,
				entity.base.z + CHAT_RADIUS  );
				
	//Broadcast to players
	mailbox->broadcast_chat(r, ss.str(), true);
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
		(entity.base.flags & EntityFlags::Inactive) || 
		abs(entity.base.x - (chunk_id.x + .5) * CHUNK_X) > (UPDATE_RADIUS+CHUNK_X) ||
		abs(entity.base.y - (chunk_id.y + .5) * CHUNK_Y) > (UPDATE_RADIUS+CHUNK_Y) ||
		abs(entity.base.z - (chunk_id.z + .5) * CHUNK_Z) > (UPDATE_RADIUS+CHUNK_Z) )
	{
		return 0;
	}
	
	//Refresh all existing entities in the chunk region for the client
	struct Visitor
	{
		EntityID player_id;
		Mailbox* mailbox;
		
		static EntityUpdateControl call(Entity& entity, void* data)
		{
			Visitor* v = (Visitor*)data;
			v->mailbox->send_entity(v->player_id, entity);
			return EntityUpdateControl::Continue;
		}
	};
	
	Visitor V = { player_id, mailbox };
	entity_db->foreach(Visitor::call, &V, Region(chunk_id), vector<EntityType>(), true);
	

	//Read the chunk from the map
	Chunk chunk;
	game_map->get_chunk(chunk_id, &chunk);
	return chunk.compress((void*)buf, buf_len);
}


//---------------------------------------------------------------
// Heartbeat
//---------------------------------------------------------------

//Implementation of heartbeat function
bool heartbeat_impl(Mailbox* mailbox, EntityDB* entity_db, EntityID const& player_id, int tick_count, int socket)
{
	//Sanity check player
	Entity player;
	if( !entity_db->get_entity(player_id, player) ||
		player.base.type != EntityType::Player ||
		(player.base.flags & EntityFlags::Inactive) )
	{
		cout << "Invalid player" << endl;
		return false;
	}
	
	//First, we need to make a local copy of the player packet and clear out the old one
	Mailbox::PlayerRecord data;
	{
		Mailbox::PlayerLock P(mailbox, player_id);
		Mailbox::PlayerRecord* ndata = P.data();
		if(ndata == NULL)
			return false;

		mailbox->update_index(ndata, player.base.x, player.base.y, player.base.z);
		ndata->tick_count = tick_count;
		
		data.swap(*ndata);
	}
	
	//Next, we need to poll all pollable entities within the update radius of the client
	//This is done without locking the entity database, so we could be preempted
	ScopeTCQuery Q(entity_db->entity_db);
	
	Region r(	player.base.x - UPDATE_RADIUS, 
				player.base.y - UPDATE_RADIUS,
				player.base.z - UPDATE_RADIUS,
				player.base.x + UPDATE_RADIUS,
				player.base.y + UPDATE_RADIUS,
				player.base.z + UPDATE_RADIUS );
	entity_db->add_range_query(Q.query, r);
	
	tctdbqryaddcond(Q.query, "poll", TDBQCSTREQ, "1");
	
	ScopeTCList L(tctdbqrysearch(Q.query));
	
	cout << "L.list = " << (void*)L.list << endl;
	
	if(L.list != NULL)
	while(true)
	{
		int sz;
		ScopeFree G(tclistpop(L.list, &sz));
		
		cout << "G.ptr = " << G.ptr << ", sz = " << sz << endl;
		if(G.ptr == NULL || sz != sizeof(EntityID))
			break;

		EntityID entity_id = *(EntityID*)G.ptr;
		
		cout << "Sending entity: " << entity_id.id << endl;
		Entity entity;
		if(!entity_db->get_entity(entity_id, entity))
			continue;
			
		data.serialize_entity(entity);
	}
			
	//Finally, we serialize the packet to the network
	data.net_serialize(socket);
	return true;
}

//Sends queued messages to client
//Note: This function is magical and breaks all sorts of abstractions in order to avoid
//doing many wasteful memcpy's/locks all over the place.
bool World::heartbeat(
	EntityID const& player_id,
	int socket)
{
	return heartbeat_impl(mailbox, entity_db, player_id, tick_count, socket);
}


//---------------------------------------------------------------
// Entity event handler functions
//---------------------------------------------------------------

void World::CreateHandler::call(Entity const& entity)
{
	if(entity.base.flags & EntityFlags::Inactive)
		return;

	Region r( entity.base.x - UPDATE_RADIUS, 
			  entity.base.y - UPDATE_RADIUS,
			  entity.base.z - UPDATE_RADIUS,
			  entity.base.x + UPDATE_RADIUS, 
			  entity.base.y + UPDATE_RADIUS,
			  entity.base.z + UPDATE_RADIUS);

	world->mailbox->broadcast_entity(r, entity);
}

void World::UpdateHandler::call(Entity const& entity)
{
	if( (entity.base.flags & EntityFlags::Inactive) ||
		(entity.base.flags & EntityFlags::Temporary) )
		return;

	//Polling entities only get updated when we do a heartbeat.
	if( !(entity.base.flags & EntityFlags::Poll) )
	{
		Region r( entity.base.x - UPDATE_RADIUS, 
				  entity.base.y - UPDATE_RADIUS,
				  entity.base.z - UPDATE_RADIUS,
				  entity.base.x + UPDATE_RADIUS, 
				  entity.base.y + UPDATE_RADIUS,
				  entity.base.z + UPDATE_RADIUS);
	
		world->mailbox->broadcast_entity(r, entity);		
	}
	
	//Update origin for players
	if(entity.base.type == EntityType::Player)
	{
		world->mailbox->set_origin(entity.entity_id, 
			entity.base.x, 
			entity.base.y, 
			entity.base.z);
	}
}

void World::DeleteHandler::call(Entity const& entity)
{
	if((entity.base.flags & EntityFlags::Temporary))
		return;
	
	Region r( entity.base.x - UPDATE_RADIUS, 
			  entity.base.y - UPDATE_RADIUS,
			  entity.base.z - UPDATE_RADIUS,
			  entity.base.x + UPDATE_RADIUS, 
			  entity.base.y + UPDATE_RADIUS,
			  entity.base.z + UPDATE_RADIUS);

	world->mailbox->broadcast_kill(r, entity.entity_id);
}


//---------------------------------------------------------------
// World ticking / update
//---------------------------------------------------------------

//Ticks the server
void World::tick()
{
	//Increment tick counter
	mailbox->set_tick_count(++tick_count);
	
	tick_players();
	tick_mobs();
	
	//TODO: Add tick events for other major game systems here
}

//Player loop:  Updates all players in the game
void World::tick_players()
{
	struct Visitor
	{
		World*				world;
		
		static EntityUpdateControl call(Entity& entity, void* data)
		{
			Visitor *V = (Visitor*)data;
		
			/*
			//Check for timed out players
			if(V->world->tick_count - entity.player.net_last_tick > PLAYER_TIMEOUT)
			{
				cout << "Player timeout!" << endl;
				entity.base.flags = EntityFlags::Inactive;
				V->world->mailbox->del_player(entity.entity_id);
				return EntityUpdateControl::Update;
			}
			*/
			
		
			//Update network state
			//TODO: Add some interpolation / sanity checking here.
			//	Need to guard against speed hacking
			entity.base.x = entity.player.net_x;
			entity.base.y = entity.player.net_y;
			entity.base.z = entity.player.net_z;
			
			entity.base.pitch = entity.player.net_pitch;
			entity.base.yaw = entity.player.net_yaw;
			entity.base.roll = 0.0;
			
			return EntityUpdateControl::Update;				
		}
	};
	
	Visitor V = { this };
	entity_db->foreach(
		Visitor::call, 
		&V,
		Region(),
		vector<EntityType>({ EntityType::Player }), 
		true); 
}

void World::tick_mobs()
{
	//Updates all mobs in the game (not yet implemented)
}

};
