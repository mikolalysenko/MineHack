#ifndef PLAYER_H
#define PLAYER_H

#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>

#include <tcutil.h>
#include <tchdb.h>

#include "entity.h"
#include "update_event.h"

//Player input constants
#define INPUT_FORWARD		(1<<0)
#define INPUT_BACKWARD		(1<<1)
#define INPUT_LEFT			(1<<2)
#define INPUT_RIGHT			(1<<3)
#define INPUT_JUMP			(1<<4)
#define INPUT_CROUCH		(1<<5)

namespace Game
{
	//The player state data structure
	//Stores both persistent information about player as well as network client data
	struct PlayerState
	{
		//Player entity id
		EntityID			entity_id;
	
		//Current key presses
		int 				input_state;
		
		//If set, player is logged in
		bool 				logged_in;
	};
	
	//Player iterator return value
	enum class PlayerUpdate : int
	{
		None	= 0,
		Update	= (1 << 0),
		Delete	= (1 << 1)
	};
	
	//Player update call back
	typedef PlayerUpdate (*update_player_callback)(PlayerState& state, void* user_data);
	
	//The player database
	struct PlayerDB
	{
		PlayerDB(std::string const& path);
		~PlayerDB();
	
		//Creates a player record
		bool create_player(std::string const& player_name);
		
		//Removes a player
		void remove_player(std::string const& player_name);
		
		//Retrieves player state
		bool get_state(std::string const& player_name, PlayerState& state);
		
		//Updates the player state
		bool update_player(std::string const& player_name, update_player_callback callback, void* user_data);		
		
	private:
	
		TCHDB*	player_db;
	};
};

#endif

