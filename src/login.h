#ifndef LOGIN_H
#define LOGIN_H

#include <string>

//Constants for different buffer sizes

#define PASSWORD_HASH_LEN		64
#define USERNAME_MAX_LEN		20
#define USERNAME_MIN_LEN		3


namespace Server
{
	void init_login();

	void shutdown_login();
	
	//Resynchronizes the login database
	void sync_login();
	
	bool verify_user_name(std::string const& name, std::string const& password_hash);

	bool create_account(std::string const& name, std::string const& password_hash);

	void delete_account(std::string const& name);
};

#endif

