#include <string>
#include <cstdio>

#include <stdint.h>

#include <tbb/task.h>
#include <tbb/blocked_range.h>
#include <tbb/blocked_range3d.h>
#include <tbb/parallel_for.h>

#include "network.pb.h"

#include "constants.h"
#include "misc.h"
#include "config.h"
#include "session.h"
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
World::World(Config* cfg) : config(cfg), synchronize(false)
{
	//Restore tick count
	ticks = config->readInt("tick_count");
	
	session_manager = new SessionManager();
	entity_db = new EntityDB(config);
	game_map = new GameMap(config);
}

//Clean up/saving stuff
World::~World()
{
	delete session_manager;
	delete entity_db;
	delete game_map;
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
	world_task->destroy(*world_task);
		
	DEBUG_PRINTF("World instance stopped\n");
}

//Synchronize world state
void World::sync()
{
	synchronize = true;
}

void World::main_loop()
{
	//FIXME: Spawn the map precache worker
	DEBUG_PRINTF("Starting world instance\n");
	while(running)
	{
		//Increment the global tick count
		++ticks;
		
		//FIXME: Update map physics in parallel
	
		//Iterate over the player set
		parallel_for(session_manager->sessions.range(), 
			[=](concurrent_unordered_map<SessionID, Session*>::range_type sessions)
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
					if((tick_count::now() - session->last_activity).seconds() > SESSION_TIMEOUT)
					{
						DEBUG_PRINTF("Pending session timeout, session id = %ld", session->session_id);
						session_manager->remove_session(session->session_id);
						return;
					}
				
					return;
				}
			}
			
			//Check if the sockets died
			if(!session->update_socket->alive() || !session->map_socket->alive())
			{
				DEBUG_PRINTF("Socket died, client lost connection\n");
				session_manager->remove_session(session->session_id);
				return;
			}
			
			//Handle input
			bool active = false;
			Network::ClientPacket *input_packet;
			while(session->update_socket->recv_packet(input_packet))
			{
				DEBUG_PRINTF("Got an update packet from session %ld\n", session->session_id);
				
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
					ScopeFree escape_str(tcxmlescape(input_packet->chat_message().c_str()));
					string chat_string = "<color='yellow'>" + session->player_name + "</color>: <color='white'>" + string((char*)escape_str.ptr) + "<br/>";
					broadcast_message(chat_string);
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
			if((tick_count::now() - session->last_activity).seconds() > SESSION_TIMEOUT)
			{
				DEBUG_PRINTF("Session timeout, session id = %ld", session->session_id);
				session_manager->remove_session(session->session_id);
				return;
			}
			
			//Send chunk updates to player
			send_chunk_updates(session);
		});
		
		
		//Process all pending deletes
		session_manager->process_pending_deletes();
		
		//Save the state of the world
		if(synchronize)
		{
			save_state();
		}
	}
	
	DEBUG_PRINTF("Stopping world instance\n");
	
	session_manager->clear_all_sessions();
	save_state();
}

//Send a list of session updates to the user in parallel
void World::send_chunk_updates(Session* session)
{
	//Figure out which chunks are visible
	ChunkID chunk(session->player_coord);
	
	//DEBUG_PRINTF("Player chunk = %d, %d, %d\n", chunk.x, chunk.y, chunk.z);
	
	//Scan all chunks in visible radius
	int r = config->readInt("vis_radius");
	parallel_for(blocked_range3d<int,int,int>(
		chunk.x-r, chunk.x+r,
		chunk.z-r, chunk.z+r,
		chunk.y-r, chunk.y+r), [=](blocked_range3d<int,int,int> rng)
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

void World::save_state()
{
	//FIXME:  This is not yet implemented
}

};
