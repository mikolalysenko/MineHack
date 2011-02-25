#ifndef SESSION_H
#define SESSION_H

#include <iostream>
#include <cstdint>
#include <string>
#include <vector>

#include "constants.h"
#include "entity.h"

namespace Server
{
	typedef uint64_t SessionID;
	
	//The session database
	struct SessionDB
	{
		SessionDB(std::string const& filename);
		~SessionDB();
		
		//Basic session management stuff
		bool create_session(std::string const& user_name, SessionID&);
		void delete_session(SessionID const&);
		bool logged_in(std::string const& user_name);
		
		//Session data retrieval functions
		bool get_session_data(SessionID const&, Login::Session&);
		bool update_session_data(SessionID const&, Login::Session const& prev, Login::Session& next);
	};
	
}

#endif
