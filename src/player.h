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
	//The player data structure
	struct Player
	{
		float max_block_distance;
	
		//Player name
		std::string			name;
		
		//Player position
		double				pos[3];
		
		//View direction
		float				pitch, yaw;
		
		//Current key presses
		int 				input_state;
		
		Player(std::string const& n) :
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

