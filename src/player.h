#ifndef PLAYER_H
#define PLAYER_H

#include <vector>
#include <string>
#include <cstdint>

#include "inventory.h"

#include "session.h"

#include "update_event.h"

namespace Game
{	
	//The player data structure
	struct Player
	{
		float max_block_distance;
	
		//Session id for player
		Server::SessionID	session_id;
		
		//Player name
		std::string			name;
		
		//Player position
		double				pos[3];
		
		//View direction
		float				pitch, yaw;
		
		//Current key presses
		int 				input_state;
		
		//Update events
		std::vector<UpdateEvent> updates;
		
		Player(Server::SessionID const& s, std::string const& n) :
			session_id(s), 
			name(n),
			pitch(0),
			yaw(0),
			max_block_distance(10000000)
		{ pos[0] = (1<<20); pos[1] = (1<<20); pos[2] = (1<<20); }
		
		~Player()
		{ }
	};
};

#endif

