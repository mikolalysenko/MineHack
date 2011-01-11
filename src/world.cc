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
void World::add_event(InputEvent* ev)
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
		
		int q = up->write(buf, buf_len);
		
		if(q < 0)
			break;
		
		buf += q;
		buf_len -= q;
		l += q;
		delete up;
	}
	//erase old updates
	p->updates.erase(p->updates.begin(), iter);
	

	return l;
}

//Broadcasts an event to all players
void World::broadcast_update(UpdateEvent* up, double x, double y, double z, double radius)
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

void World::handle_add_player(Server::SessionID const& session_id, JoinEvent const& ev)
{
	cout << "Adding player: " << ev.name << ", session id: " << session_id << endl << "-----------------------------------------------------------" << endl;
	players[session_id] = new Player(session_id, ev.name);
}

void World::handle_remove_player(Server::SessionID const& session_id)
{
	cout << "Removing player: " << session_id << endl;
	auto iter = players.find(session_id);
	if(iter == players.end())
		return;
	players.erase(iter);
}


void World::handle_set_block(Server::SessionID const& session_id,  BlockEvent const& ev)
{
	int x = ev.x, y = ev.y, z = ev.z;
	auto b = ev.b;

	cout << "Setting block: " << x << "," << y << "," << z << " <- " << (uint8_t)b << "; by " << session_id << endl;

	//Check range on x,y,z
	if(x < 0 || y < 0 || z < 0 ||
		x >= MAX_MAP_X || y >= MAX_MAP_Y || z >= MAX_MAP_Z)
		return;

	//Check validity
	auto iter = players.find(session_id);
	if(iter == players.end())
		return;
	auto p = (*iter).second;
		
	//Check block distance
	auto pos = p->pos;
	double d = max(max(abs(x - pos[0]), abs(y - pos[1])), abs(z - pos[2]));
	if(d < p->max_block_distance)
	{
		//Set block and broadcast event
		game_map->set_block(x, y, z, b);
		
		UpdateEvent *up = new UpdateEvent();
		up->type = UpdateEventType::SetBlock;
		up->block_event.x = x;
		up->block_event.y = y;
		up->block_event.z = z;
		up->block_event.b = b;
		
		broadcast_update(up, x, y, z, 256.0);
	}
}

void World::handle_dig_block(SessionID const& session_id, DigEvent const& dig)
{
	cout << "Got dig event: " << dig.x << "," << dig.y << "," << dig.z << endl;
	
	auto iter = players.find(session_id);
	if(iter == players.end())
		return;
	auto p = (*iter).second;
	
	int x = p->pos[0] + dig.x,
		y = p->pos[1] + dig.y,
		z = p->pos[1] + dig.z;
	
	//Set block and broadcast event
	game_map->set_block(x, y, z, Block::Air);
	
	UpdateEvent *up = new UpdateEvent();
	up->type = UpdateEventType::SetBlock;
	up->block_event.x = x;
	up->block_event.y = y;
	up->block_event.z = z;
	up->block_event.b = Block::Air;
	
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
		auto ev = *iter;
		switch(ev->type)
		{
			case InputEventType::PlayerJoin:
				handle_add_player(ev->session_id, ev->join_event);
			break;
		
			case InputEventType::PlayerLeave:
				handle_remove_player(ev->session_id);
			break;
		
			case InputEventType::SetBlock:
				handle_set_block(ev->session_id, ev->block_event);
			break;
		
			case InputEventType::DigBlock:
				handle_dig_block(ev->session_id, ev->dig_event);
			break;
		
			default:
			break;
		}
		delete ev;
	}
	
	//Update map
}

};
