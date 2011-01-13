#include <string>
#include <map>
#include <iostream>
#include <cctype>
#include <cstdlib>
#include <cstdint>

//Tokyo cabinet
#include <tcutil.h>
#include <tchdb.h>

#include "misc.h"
#include "login.h"

using namespace std;

namespace Server
{

TCHDB *login_db = NULL;

//Initializes the login table
void init_login()
{
	//Create login database
	login_db = tchdbnew();
	
	//Set options
	//TODO: Set other tuning options here
	tchdbsetmutex(login_db);
	
	//Open it
	tchdbopen(login_db, "data/login.tc", HDBOWRITER | HDBOCREAT);
}

//Shuts down the login server
void shutdown_login()
{
	tchdbclose(login_db);
	tchdbdel(login_db);
}

void sync_login()
{
	tchdbsync(login_db);
}


//Check if a user name is valid
bool is_valid_username(const string& name)
{
	if(name.size() < USERNAME_MIN_LEN ||
		name.size() > USERNAME_MAX_LEN)
	{
		return false;
	}
	
	for(size_t i=0; i<name.size(); i++)
	{
		if(!isalnum(name[i]))
			return false;
	}
	
	return true;
}

//Check if a password hash is valid
bool is_valid_password_hash(const string& pass)
{
	if(pass.size() != PASSWORD_HASH_LEN)
	{
		return false;
	}
	for(int i=0; i<PASSWORD_HASH_LEN; i++)
	{
		if(!(isalnum(pass[i]) || pass[i] == '/' || pass[i] == '+'))
			return false;
	}
	return true;
}



//Verifies a user name/password combo
bool verify_user_name(const string& name, const string& password_hash)
{
	char pass_buf[PASSWORD_HASH_LEN];
	
	if(!is_valid_password_hash(password_hash) || 
		!is_valid_username(name))
	{
		return false;
	}
	
	int l = tchdbget3(login_db, 
		(const void*)name.c_str(), name.size(), 
		(void*)&pass_buf, PASSWORD_HASH_LEN);
		
	if(l != PASSWORD_HASH_LEN)
	{
		return false;
	}
	
	for(int i=0; i<PASSWORD_HASH_LEN; i++)
	{
		if(pass_buf[i] != password_hash[i])
			return false;
	}
	
	return true;
}

//Creates an account
bool create_account(const string& name, const string& password_hash)
{
	//Validate name and password
	if(!is_valid_username(name) || !is_valid_password_hash(password_hash))
	{
		return false;
	}

	return tchdbputkeep(login_db, 
		(const void*)name.c_str(), name.size(), 
		(const void*)password_hash.c_str(), password_hash.size());
}


};


