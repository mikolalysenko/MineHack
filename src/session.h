#ifndef SESSION_H
#define SESSION_H

#include <pthread.h>

#include <string>
#include <stdint.h>

#include <tbb/tick_count.h>
#include <tbb/concurrent_queue.h>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/concurrent_hash_map.h>

#include "constants.h"
#include "config.h"
#include "entity.h"
#include "httpserver.h"

namespace Game
{
	//Session ID
	typedef uint64_t SessionID;

	//A player's session information
	struct Session
	{
		Session(SessionID const& id, std::string const& player_name);
		~Session();
	
		//Basic session state information
		SessionID			session_id;
		bool				dead;				//If set, this session is marked for deletion
		tbb::tick_count		last_activity;
	
		//Player state/entity information
		std::string			player_name;	
		
		//Web socket connections
		WebSocket			*update_socket;
		WebSocket			*map_socket;
	};

	//Session manager
	struct SessionManager
	{
		SessionManager();
		~SessionManager();
		
		//Creates a new session
		bool create_session(std::string const& player_name, SessionID& session_id);
		
		//Socket attachment
		bool attach_update_socket(SessionID const& session_id, WebSocket* socket);
		bool attach_map_socket(SessionID const& session_id, WebSocket* socket);
		
		//Session removal
		void remove_session(SessionID const& session_id);
		
		//Must be called when no other task is accessing the session set, applies all
		//pending deletes to the session data set
		void process_pending_deletes();
		
		//Erases all sessions
		void clear_all_sessions();
		
		//The session data set
		tbb::concurrent_unordered_map<SessionID, Session*> sessions;
		
	private:

		//Pending session list
		tbb::concurrent_hash_map<SessionID, Session*>		pending_sessions;
		
		//Pending session removal events
		tbb::concurrent_queue<SessionID>	pending_erase;
		
		//Generates a random session
		SessionID generate_session_id();
	};
};

#endif

