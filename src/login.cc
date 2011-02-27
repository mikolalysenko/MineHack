#include <cstdint>
#include <cstdio>
#include <string>

#include <tchdb.h>

#include "login.pb.h"

#include "constants.h"
#include "config.h"
#include "misc.h"

using namespace std;
using namespace Game;

//Constructs the login server
LoginDB::LoginDB(Config* config_) : config(config_)
{
	login_db = tchdbnew();
	tchdbsetmutex(login_db);
	
}

//Deallocate the login database
LoginDB::~LoginDB()
{
	tchdbdel(login_db);
}

//Start login database
bool LoginDB::start()
{
	string login_db_path = config.readString("login_db_path");
	return tchdbopen(login_db, login_db_path.c_str(), HDBOWRITER | HDBOCREAT);
}

//Stop login database
void LoginDB::stop()
{
	tchdbclose(login_db);
}

//Saves the login database
void LoginDB::sync()
{
	tchdbsync(login_db);
}

//Creates an account
bool LoginDB::create_account(std::string const& user_name, std::string const& password_hash)
{
	//Create protocol buffer
	Login::Account account;
	account.set_user_name(user_name);
	account.set_password_hash(password_hash);
	
	//Allocate buffer
	int sz = account.ByteSize();
	ScopeFree buf(malloc(sz));
	
	//Serialize
	account.SerializeToArray(buf.ptr, sz);

	//Store in database
	return tchdbputkeep(login_db, 
		(const void*)user_name.c_str(), user_name.size(), 
		buf.ptr, sz);
}

//Deletes an account
void LoginDB::delete_account(std::string const& user_name)
{
	return tchdbout(login_db, (const void*)user_name.c_str(), user_name.size());
}

//Retrieves an account
bool LoginDB::get_account(std::string const& user_name, Login::Account& account)
{
	int sz;
	ScopeFree buf(tchdbget(login_db, (const void*)user_name.c_str(), user_name.size()));
	if(buf == NULL)
		return false;
	return accoun.ParseFromArray(buf.ptr, sz);
}

//Updates an account
bool update_account(std::string const& user_name, Login::Account& update)
{
	while(true)
	{
		if(!tchdbtranbegin(login_db))
		{
			return false;
		}

		int len;
		ScopeFree buf(tchdbget(login_db, (const void*)user_name.c_str(), user_name.size(), &len));
		if(buf.ptr == NULL || len < sizeof(LoginRecord))
		{
			tchdbtranabort(login_db);
			return false;
		}
		
		Login::Account prev;
		if(!prev.SerializeFromArray(buf.ptr, len))
		{
			tchdbtranabort(login_db);
			return false;
		}
		
		prev.MergeFrom(update);
		
		int n_len = prev.ByteSize();
		ScopeFree n_buf(malloc(n_len));
		prev.SerializeToArray(n_buf.ptr, n_len);
		
		if(!tchdbput(login_db, 
			(const void*)user_name.c_str(), user_name.size(), 
			n_buf.ptr, n_len))
		{
			tchdbtranabort(login_db);
			continue;
		}
		
		if(tchdbtrancommit(login_db))
			return true;
	}
}

};


