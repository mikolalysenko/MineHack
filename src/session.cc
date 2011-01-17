#include <iostream>
#include <map>
#include <string>
#include <cstdint>
#include <pthread.h>

#include "session.h"
#include "misc.h"
#include "entity.h"

using namespace std;

namespace Server
{

//Invariants that need to be enforced:
//  1. Each session id is associated to exactly one user name
//  2. Each user name has at most one session id (no multiple logins)
//	3. The selection of session ids is completely random

//Lock for the session table
static pthread_rwlock_t session_lock = PTHREAD_RWLOCK_INITIALIZER;

//Indexes
static map<string, SessionID>	user_table;
static map<Game::EntityID, SessionID>	player_table;
static map<SessionID, Session>	session_table;

//UUID generator
UUIDGenerator*	session_id_gen;

void init_sessions()
{
	session_id_gen = new UUIDGenerator();
}

bool valid_session_id(const SessionID& session_id)
{
	ReadLock L(&session_lock);
	return session_table.find(session_id) != session_table.end();
}

bool logged_in(const string& name)
{
	ReadLock L(&session_lock);
	return user_table.find(name) != user_table.end();
}

bool create_session(const string& user_name, SessionID& session_id)
{
	if( user_name.size() > USER_NAME_MAX_LEN )
		return false;
	
	//Generate session id
	session_id.id = session_id_gen->gen_uuid();
	
	//Initialize session variable
	Session s;
	s.state			= SessionState::PreJoinGame;
	s.user_name		= user_name;
	s.player_id.clear();
	
	{	WriteLock L(&session_lock);
		
		if(user_table.find(user_name) != user_table.end())
			return false;
		
		if(session_table.find(session_id) != session_table.end())
			return false;
		
		session_table[session_id] = s;
		user_table[user_name] = session_id;
	}
	
	return true;
}

void delete_session(const SessionID& session_id)
{
	WriteLock L(&session_lock);
	
	auto iter = session_table.find(session_id);
	
	if(iter != session_table.end())
	{
		auto session = (*iter).second;
	
		auto u_iter = user_table.find(session.user_name);
		if(u_iter != user_table.end())
			user_table.erase(u_iter);
		
		auto p_iter = player_table.find(session.player_id);
		if(p_iter != player_table.end())
			player_table.erase(p_iter);
	
		session_table.erase(iter);
	}
}

bool set_session_player(SessionID const& session_id, Game::EntityID const& player_id)
{
	WriteLock L(&session_lock);
	
	auto iter = session_table.find(session_id);
	if(	iter == session_table.end() ||
		(*iter).second.state != SessionState::PreJoinGame ||
		player_table.find(player_id) != player_table.end() )
		return false;

	player_table[player_id] 	= session_id;
	(*iter).second.player_id	= player_id;
	(*iter).second.state		= SessionState::InGame;
	
	return true;
}
	
bool get_session_data(SessionID const& session_id, Session& result)
{
	ReadLock L(&session_lock);
	
	auto iter = session_table.find(session_id);
	if(iter == session_table.end())
		return false;
		
	result = (*iter).second;
	return true;
}


};
