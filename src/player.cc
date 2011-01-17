#include <vector>
#include <string>
#include <cstdint>

#include <tcutil.h>
#include <tchdb.h>

#include "entity.h"
#include "update_event.h"
#include "player.h"

using namespace std;

namespace Game
{

PlayerDB::PlayerDB(string const& path)
{
	//Create player db
	player_db = tchdbnew();
	
	//Tuning options
	tchdbsetmutex(player_db);
	
	//Open file
	tchdbopen(player_db, path.c_str(), HDBOWRITER | HDBOCREAT);	
}

PlayerDB::~PlayerDB()
{
	tchdbclose(player_db);
	tchdbdel(player_db);
}


//Creates a player record
bool PlayerDB::create_player(string const& player_name)
{
	PlayerState initial_state;
	initial_state.entity_id.clear();
	initial_state.logged_in		= false;
	initial_state.input_state	= 0;
	
	//Try inserting into database
	return tchdbputkeep(player_db, player_name.c_str(), player_name.size(), &initial_state, sizeof(PlayerState));
}

//Removes a player record
void PlayerDB::remove_player(string const& player_name)
{
	tchdbout(player_db, player_name.c_str(), player_name.size());
}

//Retrieves player state
bool PlayerDB::get_state(string const& player_name, PlayerState& state)
{
	return tchdbget3(player_db, player_name.c_str(), player_name.size(), &state, sizeof(PlayerState));
}

//Updates the player state
bool PlayerDB::update_player(string const& player_name, update_player_callback callback, void* user_data)
{
	while(true)
	{
		if(!tchdbtranbegin(player_db))
		{
			return false;
		}
		
		PlayerState state;
		if(!tchdbget3(player_db, player_name.c_str(), player_name.size(), &state, sizeof(PlayerState)))
		{
			tchdbtranabort(player_db);
			return false;
		}
		
		switch((*callback)(state, user_data))
		{
			case PlayerUpdate::Update:
				tchdbput(player_db, player_name.c_str(), player_name.size(), &state, sizeof(PlayerState));
			break;
			
			case PlayerUpdate::Delete:
				tchdbout(player_db, player_name.c_str(), player_name.size());
			break;
		}
		
		if(tchdbtrancommit(player_db))
			return true;
	}
}

};

