#include <cstring>
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

//Check if a user name is valid
bool is_valid_user_name(const string& user_name)
{
	if(	user_name.size() < USER_NAME_MIN_LEN ||
		user_name.size() > USER_NAME_MAX_LEN)
	{
		return false;
	}
	
	for(size_t i=0; i<user_name.size(); i++)
	{
		if(!isalnum(user_name[i]))
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


//Checks if a player name is valid
bool is_valid_player_name(const string& player_name)
{
	if(	player_name.size() > PLAYER_NAME_MAX_LEN )
	{
		return false;
	}
	
	for(size_t i=0; i<player_name.size(); i++)
	{
		if(!isalnum(player_name[i]))
			return false;
	}
	
	return true;

}


//Verifies a user_name/password combo
bool verify_user_name(const string& user_name, const string& password_hash)
{
	if( !is_valid_password_hash(password_hash) || 
		!is_valid_user_name(user_name))
	{
		return false;
	}
	
	int len;
	ScopeFree G(tchdbget(login_db, (const void*)user_name.c_str(), user_name.size(), &len));
		
	if(G.ptr == NULL || len < sizeof(LoginRecord))
	{
		return false;
	}
	
	//Read in the login record
	LoginRecord* account = (LoginRecord*)G.ptr;
	return (strncmp(account->password_hash, password_hash.c_str(), PASSWORD_HASH_LEN) == 0);
}


bool get_login_record(const string& user_name, LoginRecord& result)
{
	int len;
	ScopeFree G(tchdbget(login_db, (const void*)user_name.c_str(), user_name.size(), &len));
		
	if(G.ptr == NULL || len < sizeof(LoginRecord))
	{
		return false;
	}
	
	
	result = *(LoginRecord*)G.ptr;
	return true;
}

//Creates an account
bool create_account(const string& user_name, const string& password_hash)
{
	//Validate name and password
	if(	!is_valid_user_name(user_name) || 
		!is_valid_password_hash(password_hash))
	{
		return false;
	}

	return tchdbputkeep(login_db, 
		(const void*)user_name.c_str(), user_name.size(), 
		(const void*)password_hash.c_str(), password_hash.size());
}

//Checks if a user name exists
bool user_name_exists(std::string const& user_name)
{
	int len = tchdbvsiz(login_db, (const void*)user_name.c_str(), user_name.size());
	return len >= sizeof(LoginRecord);
}

//Adds a player name to the list
bool add_player_name(std::string const& user_name, std::string const& player_name)
{
	if( !user_name_exists(user_name) || 
		!is_valid_player_name(player_name) )
	{
		return false;
	}
	
	return tchdbputcat(login_db,
		(const void*)user_name.c_str(), user_name.size(),
		(const void*)player_name.c_str(), player_name.size() + 1);
}

//Retrieves player names
bool get_player_names(string const& user_name, vector<string>& player_names)
{
	player_names.clear();

	int len;
	ScopeFree G(tchdbget(login_db, (const void*)user_name.c_str(), user_name.size(), &len));
	
	if(G.ptr == NULL || len < sizeof(LoginRecord))
		return false;
	
	char* ptr = ((char*)G.ptr) + sizeof(LoginRecord);
	for(int i=sizeof(LoginRecord); i<len; ++i)
	{
		if( ((char*)G.ptr)[i] == '\0' )
		{
			player_names.push_back(ptr);
			ptr = ((char*)G.ptr) + i + 1;
		}
	}
	
	return true;
}

//Removes a player name from an account	
bool remove_player_name(string const& user_name, string const& player_name)
{
	while(true)
	{
		if(!tchdbtranbegin(login_db))
		{
			return false;
		}

		int len;
		ScopeFree G(tchdbget(login_db, (const void*)user_name.c_str(), user_name.size(), &len));
		if(G.ptr == NULL || len < sizeof(LoginRecord))
		{
			tchdbtranabort(login_db);
			return false;
		}
	
	
		bool found_item = false;
		char* ptr = ((char*)G.ptr) + sizeof(LoginRecord);
		for(int i=sizeof(LoginRecord); i<len; )
		{
			cout << "Checking record: " << ptr << endl;
		
			int l = strlen(ptr) + 1;
			i += l;
			if(strcmp(ptr, player_name.c_str()) == 0)
			{
				memmove(ptr, ((char*)G.ptr) + i, len - i);
			
				tchdbput(login_db,
					(const void*)user_name.c_str(), user_name.size(),
					G.ptr, len - l);
					
				found_item = true;
				break;
			}
			ptr += l;
		}
		
		if(found_item)
		{
			if(tchdbtrancommit(login_db))
				return true;
			continue;
		}
	
		tchdbtranabort(login_db);
		return false;
	}
}

};


