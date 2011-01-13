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
		std::int64_t key;
	
		bool operator==(SessionID const& other) const
			{ return key == other.key; }
	
		int operator<(SessionID const& other) const
			{ return key < other.key; }
	};

	//IO opreations for sessionID (mostly for stringification/sending get request)
	std::ostream& operator<<(std::ostream&, SessionID const&);
	std::istream& operator>>(std::istream&, SessionID&);

	void init_sessions();
	
	bool valid_session_id(SessionID const&);
		
	bool logged_in(std::string const& username);

	bool create_session(std::string const& username, SessionID&);
	
	void delete_session(SessionID const&);
}

#endif
