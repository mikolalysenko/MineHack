#ifndef SESSION_H
#define SESSION_H

#include <iostream>
#include <cstdint>


namespace Sessions
{
	struct SessionID
	{
		std::int64_t key;
	
		//Constructors
		SessionID() : key(0) {}
		SessionID(const SessionID& other) : key(other.key) {}
	
		//Assignment operator
		SessionID& operator=(const SessionID& other)
		{	key = other.key;
			return *this;
		}
	
		bool operator==(const SessionID& other) const
			{ return key == other.key; }
	
		int operator<(const SessionID& other) const
			{ return key < other.key; }
	};

	//IO opreations for sessionID
	std::ostream& operator<<(std::ostream&, const SessionID&);
	std::istream& operator>>(std::istream&, SessionID&);

	//Information associated to a particular session
	struct Session
	{
		SessionID id;

		//Other session specific data goes here
		std::string user_name;
	};

	void init_sessions();

	Session* create_session();
	
	Session* lookup_session(SessionID);
	
	void delete_session(SessionID);
}

#endif
