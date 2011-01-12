#ifndef LOGIN_H
#define LOGIN_H

#include <string>

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

