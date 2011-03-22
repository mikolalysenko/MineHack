#ifndef WORLD_H
#define WORLD_H

#include <pthread.h>

#include <string>
#include <stdint.h>

#include <tcutil.h>

#include <tbb/atomic.h>
#include <tbb/task.h>

#include "constants.h"
#include "config.h"
#include "chunk.h"
#include "session.h"
#include "game_map.h"
#include "physics.h"

namespace Game
{
	class World
	{
	public:
						
		//Ctor
		World(Config* config);
		~World();
		
		//Event loop management
		bool start();
		void stop();
		void sync();
		
		//Player management functions
		bool player_create(std::string const& player_name);
		bool player_delete(std::string const& player_name);
		bool player_join(std::string const& player_name, SessionID& session_id);
		bool player_leave(SessionID const& session_id);
		bool player_attach_update_socket(SessionID const& session_id, WebSocket*);
		bool player_attach_map_socket(SessionID const& session_id, WebSocket*);
				
		//Task function
		void main_loop();
		
	private:

		//State variables
		tbb::atomic<bool>	running;
		uint64_t		ticks;
	
		//The world update task
		tbb::task*		world_task;
	
		//Subsystems
		Config			*config;
		SessionManager	*session_manager;
		GameMap			*game_map;
		Physics			*physics;
		
		//Player update stuff
		void update_players();
		void send_chunk_updates(Session* session, int);
		void send_world_updates(Session* session);
		
		//Broadcasts a console message
		void broadcast_message(std::string const& str);
	};
};

#endif

