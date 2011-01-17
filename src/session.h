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
	
	enum class SessionState : std::uint8_t
	{
		PreJoinGame,
		InGame
	};
	
	//Session data
	struct Session
	{
		//The session state
		SessionState	state;
		std::string 	user_name;
		Game::EntityID	player_id;
	};

	void init_sessions();
	
	void shutdown_sessions();
	
	bool valid_session_id(SessionID const&);
		
	bool logged_in(std::string const& user_name);
	
	bool create_session(std::string const& user_name, SessionID&);
	
	bool set_session_player(SessionID const&, Game::EntityID const& player_id);
	
	bool get_session_data(SessionID const&, Session&);
	
	void delete_session(SessionID const&);
	
}

#endif
