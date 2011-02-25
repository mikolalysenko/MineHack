#ifndef LOGIN_H
#define LOGIN_H

#include <vector>
#include <string>

#include "constants.h"

//Login protocol buffer stuff
#include "login.pb.h"

namespace Server
{

	//The login database
	struct LoginDB
	{
		//Constructs the login server
		LoginDB(std::string const& filename);
		~LoginDB();
	
		//Account management
		bool create_account(std::string const& user_name, std::string const& password_hash);
		void delete_account(std::string const& user_name);
	
		//Record retrieval
		bool get_account(std::string const& user_name, Login::Account& account);
		bool update_account(std::string const& user_name, 
			const Login::Account& prev_account, 
			Login::Account& next_account);
	};
};

#endif

