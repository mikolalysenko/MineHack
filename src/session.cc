#include <iostream>
#include <map>
#include <string>
#include <cstdint>
#include <pthread.h>

#include "session.h"
#include "misc.h"

using namespace std;

namespace Server
{

//Invariants that need to be enforced:
//  1. Each session id is associated to exactly one user name
//  2. Each user name has at most one session id (no multiple logins)
//	3. The selection of session ids is completely random

//Information associated to a particular session
struct Session
{
	SessionID id;

	//Other session specific data goes here
	std::string user_name;
};


//Lock for the session table
static pthread_rwlock_t session_lock = PTHREAD_RWLOCK_INITIALIZER;

//Indexes
static map<string, SessionID>	user_table;
static map<uint64_t, Session>	session_table;


void init_sessions()
{
}

bool valid_session_id(const SessionID& session_id)
{
	ReadLock L(&session_lock);
	return session_table.find(session_id.id) != session_table.end();
}

bool logged_in(const string& name)
{
	ReadLock L(&session_lock);
	return user_table.find(name) != user_table.end();
}

bool create_session(const string& name, SessionID& session_id)
{
	session_id.id = (((int64_t)mrand48())<<32) | (int64_t)mrand48();
	
	Session s;
	s.user_name = name;
	s.id = session_id;
	
	{	WriteLock L(&session_lock);
		
		if(user_table.find(name) != user_table.end())
			return false;
		
		if(session_table.find(session_id.id) != session_table.end())
			return false;
		
		session_table[session_id.id] = s;
		user_table[name] = session_id;
	}
	
	return true;
}

void delete_session(const SessionID& session_id)
{
	WriteLock L(&session_lock);
		
	auto iter = session_table.find(session_id.id);
	
	if(iter != session_table.end())
		session_table.erase(iter);
}

};
