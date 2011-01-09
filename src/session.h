#ifndef SESSION_H
#define SESSION_H

#include <iostream>
#include <cstdint>
#include <string>

namespace Server
{
	struct SessionID
	{
		std::int64_t key;
	
		bool operator==(const SessionID& other) const
			{ return key == other.key; }
	
		int operator<(const SessionID& other) const
			{ return key < other.key; }
	};

	//IO opreations for sessionID
	std::ostream& operator<<(std::ostream&, const SessionID&);
	std::istream& operator>>(std::istream&, SessionID&);

	void init_sessions();
	
	bool valid_session_id(const SessionID&);
	
	bool logged_in(const std::string& name);

	bool create_session(const std::string& name, SessionID&);
	
	void delete_session(const SessionID&);
}

#endif
