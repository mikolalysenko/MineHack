#include <string>
#include <cstdio>

#include <stdint.h>

#include <tbb/atomic.h>
#include <tbb/task.h>
#include <tbb/blocked_range.h>
#include <tbb/blocked_range3d.h>
#include <tbb/parallel_for.h>
#include <tbb/task_group.h>

#include "network.pb.h"

#include "constants.h"
#include "misc.h"
#include "config.h"
#include "session.h"
#include "game_map.h"
#include "physics.h"
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
	running = false;
	
	//Restore tick count
	ticks = config->readInt("ticks");
	
	session_manager = new SessionManager();
	game_map = new GameMap(config);
	physics = new Physics(config, game_map);
}

//Clean up/saving stuff
World::~World()
{
	delete physics;
	delete game_map;
	delete session_manager;
}

//Creates a player
bool World::player_create(string const& player_name)
{
	if(!running)
		return false;
	return true;
}

//Called to delete a player
bool World::player_delete(string const& player_name)
{
	if(!running)
		return false;
	return true;
}

//Attempt to add player to the game
bool World::player_join(string const& player_name, SessionID& session_id)
{
	if(!running)
		return false;
	return session_manager->create_session(player_name, session_id);
}

//Handle a player leave event
bool World::player_leave(SessionID const& session_id)
{
	if(!running)
		return false;
	return true;
}

//Attach an update socket
bool World::player_attach_update_socket(SessionID const& session_id, WebSocket* socket)
{
	if(!running)
		return false;
	return session_manager->attach_update_socket(session_id, socket);
}

//Attach a map socket
bool World::player_attach_map_socket(SessionID const& session_id, WebSocket* socket)
{
	if(!running)
		return false;
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
	
	//Create the thread group
	running = true;
	world_task = new (tbb::task::allocate_root()) tbb::empty_task;
	world_task->set_ref_count(2);
	auto worker = new(world_task->allocate_child()) WorldTask(this);
	world_task->spawn(*worker);
	
	return true;
}

//Stops the world
void World::stop()
{
	running = false;
	world_task->wait_for_all();
		
	DEBUG_PRINTF("World instance stopped\n");
}

//Synchronize world state
void World::sync()
{
}

void World::main_loop()
{
	//FIXME: Spawn the map precache worker
	DEBUG_PRINTF("Starting world instance\n");
	while(running)
	{
	
		//Increment the global tick count
		ticks ++;
		
		//Start physics execution asynchronously
		if((ticks % 16) == 0)
		{
			physics->update(ticks - 16);
		}
		
		//Spawn each of the main game tasks
		task_group game_tasks;
		
		game_tasks.run([&](){ update_players(); });
		
		game_tasks.wait();
		
		//Process all pending deletes
		session_manager->process_pending_deletes();
		
		//Update tick counter in database
		config->storeInt("ticks", ticks);
	}
	
	session_manager->clear_all_sessions();
}



void World::update_players()
{
	//Iterate over the player set
	double session_timeout = config->readFloat("session_timeout"),
			update_rate = config->readFloat("update_rate");
	int visible_radius = config->readInt("visible_radius");
	parallel_for(session_manager->sessions.range(), 
		[&](concurrent_unordered_map<SessionID, Session*>::range_type sessions)
	{
		auto session = sessions.begin()->second;

		//Check state
		if(session->state == SessionState_Dead)
		{
			return;
		}
		else if(session->state == SessionState_Pending)
		{
		
			if(session->map_socket != NULL && session->update_socket != NULL)
			{
				session->state = SessionState_Active;
			}
			else
			{
				if((tick_count::now() - session->last_activity).seconds() > session_timeout)
				{
					printf("Pending session timeout, session id = %ld, player name = %s\n", session->session_id, session->player_name.c_str());
					session_manager->remove_session(session->session_id);
					return;
				}
			
				return;
			}
		}
		
		//Check if the sockets died
		if(!session->update_socket->alive() || !session->map_socket->alive())
		{
			printf("Client lost connection, session id = %ld, player name = %s\n", session->session_id, session->player_name.c_str());
			session_manager->remove_session(session->session_id);
			return;
		}
		
		//Handle input
		bool active = false;
		Network::ClientPacket *input_packet;
		while(session->update_socket->recv_packet(input_packet))
		{
			if(input_packet->has_player_update())
			{
				//Unpack update and set the new coordinate for the player
				if(input_packet->player_update().has_x())
					session->player_coord.x = input_packet->player_update().x();
				if(input_packet->player_update().has_y())
					session->player_coord.y = input_packet->player_update().y();
				if(input_packet->player_update().has_z())
					session->player_coord.z = input_packet->player_update().z();
			}
			else if(input_packet->has_chat_message())
			{
				//Broadcast chat message
				DEBUG_PRINTF("Got a chat message\n");
				ScopeFree escape_str(tcxmlescape(input_packet->chat_message().c_str()));
				string chat_string = "<font color='yellow'>" + session->player_name + " :</font> <font color='white'>" + string((char*)escape_str.ptr) + "</font><br/>";
				broadcast_message(chat_string);
			}
			else if(input_packet->has_block_update() &&
				input_packet->block_update().has_x() &&
				input_packet->block_update().has_y() &&
				input_packet->block_update().has_z() &&
				input_packet->block_update().has_block() )
			{
				DEBUG_PRINTF("Updating block\n");
				set_block(
					Block(input_packet->block_update().block()),
					ticks,
					input_packet->block_update().x(),
					input_packet->block_update().y(),
					input_packet->block_update().z());
			}
			
			active = true;
			delete input_packet;
		}
		
		//Update the activity flag
		if(active)
		{
			session->last_activity = tick_count::now();
		}
		
		//Check for time out condition
		if((tick_count::now() - session->last_activity).seconds() > session_timeout)
		{
			printf("Session timeout, session id = %ld, player name = %s", session->session_id, session->player_name.c_str());
			session_manager->remove_session(session->session_id);
			return;
		}
		
		//Send chunk updates to player
		send_chunk_updates(session, visible_radius);
		
		//Send game state updates to player
		if((tick_count::now() - session->last_updated).seconds() > update_rate)
		{
			send_world_updates(session);
			session->last_updated = tick_count::now();
		}
	});
}

//Send a list of session updates to the user in parallel
void World::send_chunk_updates(Session* session, int r)
{
	//Figure out which chunks are visible
	ChunkID chunk(session->player_coord);
	
	auto coord = session->player_coord;
	
	/*
	DEBUG_PRINTF("Player chunk = %d, %d, %d; position = %d,%d,%d\n", chunk.x, chunk.y, chunk.z,
		(int)coord.x, (int)coord.y, (int)coord.z);
	*/
	
	//Scan all chunks in visible radius
	parallel_for(blocked_range3d<int,int,int>(
		chunk.x-r, chunk.x+r,
		chunk.z-r, chunk.z+r,
		chunk.y-r, chunk.y+r), [&](blocked_range3d<int,int,int> rng)
	{
		for(auto iy = rng.cols().begin();  iy!=rng.cols().end();  ++iy)
		for(auto iz = rng.rows().begin();  iz!=rng.rows().end();  ++iz)
		for(auto ix = rng.pages().begin(); ix!=rng.pages().end(); ++ix)
		{
			ChunkID chunk_id(ix, iy, iz);
			
			//Check if ID is known by player
			auto iter = session->known_chunks.find(chunk_id);
			uint64_t last_seen = 0;
			if(iter != session->known_chunks.end())
				last_seen = iter->second;
			
			//If not, send the packet to the player
			auto packet = game_map->get_net_chunk(chunk_id, last_seen);
			if(packet != NULL)
			{
				session->known_chunks.insert(make_pair(chunk_id, packet->chunk_response().last_modified()));
				session->map_socket->send_packet(packet);
			}
		}
	});
}

//Sends world updates to the player
void World::send_world_updates(Session* session)
{
	DEBUG_PRINTF("Updating player: %s\n", session->player_name.c_str());

	auto packet = new Network::ServerPacket();
	packet->mutable_world_update()->set_ticks(ticks);
	session->update_socket->send_packet(packet);
}

//Broadcasts a chat message to all players
void World::broadcast_message(std::string const& str)
{
	printf("%s\n", str.c_str());
	
	for(auto iter = session_manager->sessions.begin();
		iter != session_manager->sessions.end();
		++iter)
	{
		auto packet = new Network::ServerPacket();
		packet->set_chat_message(str);
		iter->second->update_socket->send_packet(packet);
	}
}

//Sets a block in the world
void World::set_block(Block b, uint64_t t, int x, int y, int z)
{
	//Push the block to the physics simulator
	physics->set_block(b, t, x, y, z);
}


};
