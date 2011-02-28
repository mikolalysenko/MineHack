#ifndef LOGIN_H
#define LOGIN_H

#include <string>
#include <tchdb.h>
#include "login.pb.h"
#include "config.h"

namespace Game {

	//The login database
	struct LoginDB
	{
		//Constructs the login server
		LoginDB(Config* config);
		~LoginDB();
		
		//Initialization/state management
		bool start();
		void stop();
		void sync();
	
		//Account management
		bool create_account(std::string const& user_name, std::string const& password_hash);
		void delete_account(std::string const& user_name);
	
		//Record retrieval/update
		bool get_account(std::string const& user_name, Login::Account& account);
		bool update_account(std::string const& user_name, Login::Account& next);
	
	private:
		Config* config;
		TCHDB*	login_db;
	};
};

#endif

