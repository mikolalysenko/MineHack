#include <cstdlib>
#include "session.h"

using namespace std;
using namespace tbb;
using namespace Game;


Session::Session(SessionID const& id, string const& name) :
	session_id(id),
	dead(false),
	last_activity(tick_count::now()),
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
	
	concurrent_hash_map<SessionID, Session*>::accessor acc;
	if(!pending_sessions.insert(acc, session_id))
		return false;
		
	acc->second = new Session(session_id, player_name);
	return true;
}

//Socket attachment
bool SessionManager::attach_update_socket(SessionID const& session_id, WebSocket* socket)
{
	concurrent_hash_map<SessionID, Session*>::accessor acc;
	if(!pending_sessions.find(acc, session_id))
		return false;
		
	if(acc->second->update_socket != NULL)
		return false;
		
	acc->second->last_activity = tick_count::now();
	acc->second->update_socket = socket;
	
	if(acc->second->map_socket != NULL)
	{
		auto piter = sessions.insert(make_pair(session_id, acc->second));
		assert(piter.second);
		pending_sessions.erase(acc);
	}
	
	return true;
}

bool SessionManager::attach_map_socket(SessionID const& session_id, WebSocket* socket)
{
	concurrent_hash_map<SessionID, Session*>::accessor acc;
	if(!pending_sessions.find(acc, session_id))
		return false;
		
	if(acc->second->map_socket != NULL)
		return false;
		
	acc->second->last_activity = tick_count::now();
	acc->second->map_socket = socket;
	
	if(acc->second->update_socket != NULL)
	{
		sessions.insert(make_pair(session_id, acc->second));
		pending_sessions.erase(acc);
	}
	
	return true;
}

//Session removal
void SessionManager::remove_session(SessionID const& session_id)
{
	if(!pending_sessions.erase(session_id))
	{
		auto iter = sessions.find(session_id);
		if(iter == sessions.end())
			return;
		iter->second->dead = true;
		pending_erase.push(session_id);
	}
}

//Must be called when no other task is accessing the session set, applies all
//pending deletes to the session data set
void SessionManager::process_pending_deletes()
{
	SessionID session_id;
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
	for(auto iter = sessions.begin(); iter!=sessions.end(); ++iter)
	{
		delete iter->second;
	}
	sessions.clear();
}
		

//FIXME:  This is not thread safe and is stupid
SessionID SessionManager::generate_session_id()
{
	return mrand48();
}

