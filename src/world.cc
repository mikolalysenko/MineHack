#include <pthread.h>

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
	auto gen = new WorldGen();
	game_map = new Map(gen, "data/map.tc");
	
	pthread_mutex_init(&event_lock, NULL);
	pthread_mutex_init(&world_lock, NULL);
}

//Clean up/saving stuff
void World::shutdown()
{
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
	//Read the chunk from the map
	Chunk chunk;
	game_map->get_chunk(chunk_id, &chunk);
	
	//Return the compressed chunk
	return chunk.compress((void*)buf, buf_len);
}
	
//Sends queued messages to client
int World::heartbeat(
	Server::SessionID const& session_id,
	uint8_t* buf,
	size_t buf_len)
{
	vector<UpdateEvent> update_queue;
	
	//Need to acquire world lock in order to read player's pending events
	{	MutexLock wl(&world_lock);

		auto piter = players.find(session_id);
		if(piter == players.end())
		{
			cout << "Player does not exist?" << endl;
			return -1;
		}
		update_queue.swap((*piter).second->updates);
	}

	//Write the update events out to the buffer
	int l = 0;
	auto iter = update_queue.begin();
	for(; iter != update_queue.end(); ++iter)
	{
		auto up = *iter;
		
		int q = up.write(buf, buf_len);
		
		if(q < 0)
			break;
		
		buf += q;
		buf_len -= q;
		l += q;
	}
	
	//If there are any pending updates that didn't fit in the buffer, push them back to the player
	if(iter != update_queue.end())
	{
		cout << "Did not send all updates to player" << endl;
		
		MutexLock wl(&world_lock);
		
		auto piter = players.find(session_id);
		if(piter == players.end())
		{
			cout << "Player does not exist?" << endl;
			return -1;
		}
		
		//Append updates to the front of the player's update queue
		auto p = (*piter).second;
		p->updates.reserve(p->updates.size() + update_queue.size());
		p->updates.insert(p->updates.begin(), update_queue.begin(), update_queue.end());
	}
	
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

void World::handle_chat(Player* p, ChatEvent const& chat)
{
	//Create null terminated buffer for pretty printing
	char buffer[129];
	memcpy(buffer, chat.msg, chat.len);
	buffer[chat.len] = 0;
	
	cout << p->name << " says: " << buffer << endl;
	
	//TODO: Filtering/sanity check
	if(p->name.size() > 20 || chat.len > 128)
		return;
	
	UpdateEvent up;
	up.type = UpdateEventType::Chat;
	
	up.chat_event.name_len = p->name.size();
	memcpy(up.chat_event.name, p->name.c_str(), up.chat_event.name_len);
	up.chat_event.msg_len = chat.len;
	memcpy(up.chat_event.msg, chat.msg, chat.len);
	
	broadcast_update(up, p->pos[0], p->pos[1], p->pos[2], 256.0);
};




//Ticks the server
void World::tick()
{
	//Copy pending events to local queue
	events.clear();
	{
		MutexLock lock(&event_lock);
		events.swap(pending_events);
	}
	
	//Acquire a lock on the game world
	MutexLock pl(&world_lock);
	
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
			
			case InputEventType::Chat:
				handle_chat(p, ev.chat_event);
			break;
		
			default:
			break;
		}
	}
	
	//Process player inputs
	
	//Update map
}

};
