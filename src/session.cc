#include <string>
#include <stdint.h>

#include <tbb/tick_count.h>
#include <tbb/spin_rw_mutex.h>
#include <tbb/concurrent_queue.h>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/concurrent_hash_map.h>

#include "constants.h"
#include "config.h"
#include "httpserver.h"
#include "chunk.h"
#include "entity.h"
#include "session.h"

using namespace std;
using namespace tbb;
using namespace Game;

#define SESSION_DEBUG 1

#ifndef SESSION_DEBUG
#define DEBUG_PRINTF(...)
#else
#define DEBUG_PRINTF(...)  fprintf(stderr,__VA_ARGS__)
#endif

Session::Session(SessionID const& id, string const& name) :
	session_id(id),
	state(SessionState_Pending),
	last_activity(tick_count::now()),
	last_updated(tick_count::now()),
	player_name(name),
	update_socket(NULL),
	map_socket(NULL)
{
}

Session::~Session()
{
	if(update_socket != NULL)
		delete update_socket;
	if(map_socket != NULL)
		delete map_socket;
}

//Session manager constructor
SessionManager::SessionManager()
{
}

//Destructor
SessionManager::~SessionManager()
{
	clear_all_sessions();
}

//Creates a new session
bool SessionManager::create_session(string const& player_name, SessionID& session_id)
{
	session_id = generate_session_id();
	
	DEBUG_PRINTF("Created session, id = %ld\n", session_id);
	
	//Lock database
	queuing_rw_mutex::scoped_lock L(session_lock, false);
	if(sessions.find(session_id) != sessions.end())
	{
		//Just a sanity check, but this should be true
		return false;
	}

	sessions.insert(make_pair(session_id, new Session(session_id, player_name)));
	
	return true;
}

//Socket attachment
bool SessionManager::attach_update_socket(SessionID const& session_id, WebSocket* socket)
{
	queuing_rw_mutex::scoped_lock L(session_lock, false);
	
	auto iter = sessions.find(session_id);
	
	if(	iter == sessions.end() ||
		iter->second->update_socket != NULL )
		return false;

	DEBUG_PRINTF("Attached update socket, id = %ld\n", session_id);

	iter->second->last_activity = tick_count::now();
	iter->second->update_socket = socket;
	
	return true;
}

bool SessionManager::attach_map_socket(SessionID const& session_id, WebSocket* socket)
{
	queuing_rw_mutex::scoped_lock L(session_lock, false);
	
	auto iter = sessions.find(session_id);
	
	if(	iter == sessions.end() ||
		iter->second->map_socket != NULL )
		return false;

	DEBUG_PRINTF("Attached update socket, id = %ld\n", session_id);

	iter->second->last_activity = tick_count::now();
	iter->second->map_socket = socket;
	
	return true;	
}

//Session removal
void SessionManager::remove_session(SessionID const& session_id)
{
	auto iter = sessions.find(session_id);
	if(iter == sessions.end())
		return;
	iter->second->state = SessionState_Dead;
	pending_erase.push(session_id);
}

//Must be called when no other task is accessing the session set, applies all
//pending deletes to the session data set
void SessionManager::process_pending_deletes()
{
	SessionID session_id;
	queuing_rw_mutex::scoped_lock L(session_lock, true);
	while(pending_erase.try_pop(session_id))
	{
		auto iter = sessions.find(session_id);
		delete iter->second;
		sessions.unsafe_erase(iter);
	}
}

//Clears all pending sessions
void SessionManager::clear_all_sessions()
{
	queuing_rw_mutex::scoped_lock L(session_lock, true);
	for(auto iter = sessions.begin(); iter!=sessions.end(); ++iter)
	{
		delete iter->second;
	}
	sessions.clear();
}
		

//FIXME:  This is not thread safe and is stupid
SessionID SessionManager::generate_session_id()
{
	return ((uint64_t)rand()) | ((uint64_t)rand()<<32ULL);
}

