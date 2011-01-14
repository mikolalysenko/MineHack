#ifndef LOGIN_H
#define LOGIN_H

#include <vector>
#include <string>

//Constants for different buffer sizes

#define PASSWORD_HASH_LEN		64
#define USER_NAME_MAX_LEN		20
#define USER_NAME_MIN_LEN		3

//Player name info
#define PLAYER_NAME_MAX_LEN		20

namespace Server
{

	//The login record for the account
	struct LoginRecord
	{
		char password_hash[PASSWORD_HASH_LEN];
	};

	//Initialize login database
	void init_login();

	//Shuts down login database
	void shutdown_login();
	
	//Verifies an account
	bool verify_user_name(std::string const& user_name, std::string const& password_hash);

	//Retrieves a login record
	bool get_login_record(std::string const& user_name, LoginRecord&);

	//Creates an account
	bool create_account(std::string const& user_name, std::string const& password_hash);

	//Deletes an account
	void delete_account(std::string const& user_name);
	
	//Checks if a user name exists
	bool user_name_exists(std::string const& user_name);
	
	//Retrieves player names
	bool get_player_names(std::string const& user_name, std::vector<std::string>& player_names);
	
	//Adds a player name to the list
	bool add_player_name(std::string const& user_name, std::string const& player_name);

	//Removes a player name from an account	
	bool remove_player_name(std::string const& user_name, std::string const& player_name);
};

#endif

