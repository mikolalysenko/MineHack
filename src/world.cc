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
	auto gen = new WorldGen();
	game_map = new Map(gen, "data/map.tc");
	
	//Initialize locks
	pthread_mutex_init(&event_lock, NULL);
	pthread_mutex_init(&player_lock, NULL);
}

//Clean up/saving stuff
World::~World()
{
	delete game_map;
}

//Adds a player
bool World::add_player(std::string const& player_name)
{
	MutexLock lock(&player_lock);
	
	auto iter = players.find(player_name);
	if(iter == players.end())
	{
		players[player_name] = new Player(player_name);
		return true;
	}
	
	return false;
}

//Removes a player
bool World::remove_player(std::string const& player_name)
{
	MutexLock lock(&player_lock);
	
	auto iter = players.find(player_name);
	if(iter != players.end())
	{
		players.erase(iter);
	}
	
	return true;
}


//Adds an event to the world
void World::add_event(string const& player_name, InputEvent const& ev)
{
	MutexLock lock(&event_lock);
	pending_events.push_back(make_pair(player_name, ev));
}

//Retrieves a compressed chunk from the server
int World::get_compressed_chunk(
	string const& player_name,
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
void* World::heartbeat(
	string const& player_name,
	int& len)
{
	return player_updates.get_events(player_name, len);
}

//Broadcasts an event to all players
void World::broadcast_update(UpdateEvent const& up, double x, double y, double z, double radius)
{
	for(auto pl=players.begin(); pl!=players.end(); ++pl)
	{
		player_updates.send_event((*pl).first, up);
	}
}

//Handle placing a block
void World::handle_place_block(Player* p,  BlockEvent const& ev)
{
}

void World::handle_player_tick(Player* p, PlayerEvent const& ev)
{
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
	MutexLock pl(&player_lock);
	
	//Handle all events
	for(auto iter = events.begin(); iter!=events.end(); ++iter)
	{
		//Recover event pointer
		auto player_name 	= (*iter).first;
		auto input 			= (*iter).second;
		
		//Check player exists
		auto piter = players.find(player_name);
		if(piter == players.end())
			continue;
		auto player = (*piter).second;
		
		//Dispatch event handler
		switch(input.type)
		{
			case InputEventType::PlaceBlock:
				handle_place_block(player, input.block_event);
			break;
		
			case InputEventType::DigBlock:
				handle_dig_block(player, input.dig_event);
			break;
			
			case InputEventType::PlayerTick:
				handle_player_tick(player, input.player_event);
			break;
			
			case InputEventType::Chat:
				handle_chat(player, input.chat_event);
			break;
		
			default:
			break;
		}
	}
	
	//Process player inputs
	
	//Update map
}

};
