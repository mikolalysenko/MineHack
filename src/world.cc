#include <pthread.h>

#include <utility>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

#include "misc.h"
#include "chunk.h"
#include "world.h"

using namespace std;
using namespace Eigen;
using namespace Server;

namespace Game
{

//The world instance, handles game logic stuff
World::World()
{
	running = true;
	gen = new WorldGen();
	game_map = new Map(gen);
	
	pthread_mutex_init(&event_lock, NULL);
	pthread_mutex_init(&players_lock, NULL);
	
}

//Adds an event to the world
void World::add_event(InputEvent const& ev)
{
	MutexLock lock(&event_lock);
	pending_events.push_back(ev);
}

//Retrieves a compressed chunk from the server
int World::get_compressed_chunk(
	Server::SessionID const& session_id, 
	ChunkID const& chunk_id,
	uint8_t* buf,
	size_t buf_len)
{
	//Lock region for reading
	RegionReadLock lock(game_map, chunk_id);
	Chunk* chunk = game_map->get_chunk(chunk_id);	
	return chunk->compress((void*)buf, buf_len);
}
	
//Sends queued messages to client
int World::heartbeat(
	Server::SessionID const& session_id,
	uint8_t* buf,
	size_t buf_len)
{
	//Lock the player data base
	MutexLock pl(&players_lock);
	
	auto biter = players.begin();
	
	//Verify that player is logged in
	auto piter = players.find(session_id);
	if(piter == players.end())
	{
		cout << "Player does not exist?" << endl;
		return -1;
	}
	
	//Handle all pending updates
	auto p = (*piter).second;
	int l = 0;
	
	auto iter = p->updates.begin();
	for(; iter!=p->updates.end(); ++iter)
	{
		auto up = *iter;
		
		int q = up.write(buf, buf_len);
		
		if(q < 0)
			break;
		
		buf += q;
		buf_len -= q;
		l += q;
	}
	//erase old updates
	p->updates.erase(p->updates.begin(), iter);
	

	return l;
}

//Broadcasts an event to all players
void World::broadcast_update(UpdateEvent const& up, double x, double y, double z, double radius)
{
	for(auto pl=players.begin(); pl!=players.end(); ++pl)
	{
		auto p = (*pl).second;
		
		auto dx = p->pos[0] - x,
			 dy = p->pos[1] - y,
			 dz = p->pos[2] - z;
		
		if( dx*dx+dy*dy+dz*dz < radius*radius )
		{
			p->updates.push_back(up);
		}
	}
}

//Handle a player join event
void World::handle_add_player(Server::SessionID const& session_id, JoinEvent const& ev)
{
	cout << "Adding player: " << ev.name << ", session id: " << session_id << endl 
		 << "-----------------------------------------------------------" << endl;
	players[session_id] = new Player(session_id, ev.name);
}

//Handle removing a player
void World::handle_remove_player(Player* p)
{
	cout << "Removing player: " << p->session_id << endl;
	auto iter = players.find(p->session_id);
	if(iter == players.end())
	{
		cout << "Player had bad session id?" << endl;
		return;
	}
	players.erase(iter);
}

//Handle placing a block
void World::handle_place_block(Player* p,  BlockEvent const& ev)
{
	int x = ev.x, y = ev.y, z = ev.z;
	auto b = ev.b;

	cout << "Setting block: " << x << "," << y << "," << z << " <- " << (uint8_t)b << "; by " << p->session_id << endl;

	
	//TODO: Sanity check event
	//Future:  This event should take a reference to an inventory stack, not an arbitrary block type
	
	//Set block and broadcast event
	game_map->set_block(x, y, z, b);
	
	UpdateEvent up;
	up.type = UpdateEventType::SetBlock;
	up.block_event.x = x;
	up.block_event.y = y;
	up.block_event.z = z;
	up.block_event.b = b;
	
	broadcast_update(up, x, y, z, 256.0);
}

void World::handle_player_tick(Player* p, PlayerEvent const& ev)
{
	//TODO: Sanity check input values from client
	// apply some prediction/filtering to infer new position
	
	p->pos[0] = ev.x;
	p->pos[1] = ev.y;
	p->pos[2] = ev.z;
	p->pitch = ev.pitch;
	p->yaw = ev.yaw;	
	p->input_state = ev.input_state;
}

void World::handle_dig_block(Player* p, DigEvent const& dig)
{
	cout << "Got dig event: " << dig.x << "," << dig.y << "," << dig.z << endl;
	
	int x = dig.x,
		y = dig.y,
		z = dig.z;
		
		
	//TODO: Sanity check the dig event
	
	//Set block and broadcast event
	game_map->set_block(x, y, z, Block::Air);
	
	UpdateEvent up;
	up.type = UpdateEventType::SetBlock;
	up.block_event.x = x;
	up.block_event.y = y;
	up.block_event.z = z;
	up.block_event.b = Block::Air;
	
	broadcast_update(up, x, y, z, 128.0);
}




//Ticks the server
void World::tick()
{
	//Copy pending events to local queue
	events.clear();
	{
		MutexLock lock(&event_lock);
		events.swap(pending_events);
	}
	
	//Acquire the player database mutex
	MutexLock pl(&players_lock);
	
	//Acquire a map-wide lock
	Region r;
	RegionWriteLock rl(game_map, r);
	
	//Handle all events
	for(auto iter = events.begin(); iter!=events.end(); ++iter)
	{
		//Recover event pointer
		auto ev = *iter;
		
		//Special case:  Join events don't have a player associated to them
		if(ev.type == InputEventType::PlayerJoin)
		{
			handle_add_player(ev.session_id, ev.join_event);
			continue;
		}
		
		//Check player exists
		auto iter = players.find(ev.session_id);
		if(iter == players.end())
			continue;
		auto p = (*iter).second;
		
		//Dispatch event handler
		switch(ev.type)
		{
			case InputEventType::PlayerLeave:
				handle_remove_player(p);
			break;
		
			case InputEventType::PlaceBlock:
				handle_place_block(p, ev.block_event);
			break;
		
			case InputEventType::DigBlock:
				handle_dig_block(p, ev.dig_event);
			break;
			
			case InputEventType::PlayerTick:
				handle_player_tick(p, ev.player_event);
			break;
		
			default:
			break;
		}
	}
	
	//Update map
}

};
