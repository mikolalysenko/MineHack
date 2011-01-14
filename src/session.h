#ifndef SESSION_H
#define SESSION_H

#include <iostream>
#include <cstdint>
#include <string>

//Length of a session id string
#define SESSION_ID_STR_LEN		32

namespace Server
{
	struct SessionID
	{
		std::uint64_t id;
		
		bool operator==(const SessionID& other) const
		{
			return id == other.id;
		}
		
		bool operator<(const SessionID& other) const
		{
			return id < other.id;
		}
	};

	void init_sessions();
	
	bool valid_session_id(SessionID const&);
		
	bool logged_in(std::string const& username);

	bool create_session(std::string const& username, SessionID&);
	
	void delete_session(SessionID const&);
}

#endif
