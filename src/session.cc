#include <iostream>
#include <map>
#include <string>
#include <cstdint>
#include <pthread.h>

#include "session.h"
#include "misc.h"

using namespace std;

namespace Sessions
{

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
static map<SessionID, Session>	session_table;


std::ostream& operator<<(std::ostream& os, const SessionID& id)
{
	return os << id.key;
}

std::istream& operator>>(std::istream& is, SessionID& id)
{
	return is >> id.key;
}


void init_sessions()
{
}

bool valid_session_id(const SessionID& key)
{
	ReadLock L(&session_lock);
	return session_table.find(key) != session_table.end();
}

bool logged_in(const string& name)
{
	ReadLock L(&session_lock);
	return user_table.find(name) != user_table.end();
}

bool create_session(const string& name, SessionID& key)
{
	key.key = (((int64_t)mrand48())<<32) | (int64_t)mrand48();
	
	Session s;
	s.user_name = name;
	s.id = key;
	
	{	WriteLock L(&session_lock);
		
		auto iter = session_table.find(key);
		if(iter != session_table.end())
			return false;
		
		session_table[key] = s;
		user_table[name] = key;
	}
	
	return true;
}

void delete_session(const SessionID& key)
{
	WriteLock L(&session_lock);
		
	auto iter = session_table.find(key);
	
	if(iter != session_table.end())
		session_table.erase(iter);
}

};
