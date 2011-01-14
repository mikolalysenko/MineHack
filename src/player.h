#ifndef PLAYER_H
#define PLAYER_H

#include <vector>
#include <string>
#include <cstdint>

#include "inventory.h"

#include "session.h"

#include "update_event.h"

//Maximum name for a player/avatar

//Player input constants
#define INPUT_FORWARD		(1<<0)
#define INPUT_BACKWARD		(1<<1)
#define INPUT_LEFT			(1<<2)
#define INPUT_RIGHT			(1<<3)
#define INPUT_JUMP			(1<<4)
#define INPUT_CROUCH		(1<<5)

namespace Game
{

	//Player data values
	struct PlayerData
	{
		//The player's name
		char name[USER_NAME_MAX_LEN];
		
		//Player's session id
		Server::SessionID session_id;
		
		//Player position in world coordinates
		double x, y, z;
		
		//Player pitch/yaw
		double pitch, yaw;
		
		//Player input state
		int input_state;
	};


	//Contains a collection of players
	struct PlayerDB
	{
		
	};

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

