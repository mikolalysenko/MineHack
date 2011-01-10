#ifndef PLAYER_H
#define PLAYER_H

#include <pthread.h>
#include <string>
#include <cstdint>
#include <Eigen/Core>

#include "session.h"

namespace Game
{
	enum class PlayerState
	{
		PreInit,
		Connected,
		Disconnected	
	};
	
	//The player data structure
	struct Player
	{
		PlayerState state;
	
		//Session id for player
		Server::SessionID	session_id;
		
		//Player name
		std::string			name;
		
		//Player position
		Eigen::Vector4d		position;
		
		//View direction
		float				pitch, yaw;
		
		Player(const Server::SessionID& s, const std::string n) :
			state(PlayerState::PreInit),
			session_id(s), 
			name(n),
			position(0, 0, 0),
			pitch(0),
			yaw(0)
		{ }
		
		~Player()
		{ }
	};
};

#endif

