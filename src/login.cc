#include <stdint.h>
#include <cstdio>
#include <string>

#include <tchdb.h>

#include "login.pb.h"

#include "constants.h"
#include "config.h"
#include "misc.h"
#include "login.h"

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
	string login_db_path = config->readString("login_db_path");
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
	tchdbout(login_db, (const void*)user_name.c_str(), user_name.size());
}

//Retrieves an account
bool LoginDB::get_account(string const& user_name, string const& password_hash, Login::Account& account)
{
	int sz;
	ScopeFree buf(tchdbget(login_db, (const void*)user_name.c_str(), user_name.size(), &sz));
	if(buf.ptr == NULL)
		return false;
	if(!account.ParseFromArray(buf.ptr, sz))
		return false;
	return account.password_hash() == password_hash;
}

//Updates an account
bool LoginDB::update_account(
	string const& user_name, 
	Login::Account const& prev, 
	Login::Account const& next)
{
	//Serialize previous buffer
	int prev_len = prev.ByteSize();
	ScopeFree prev_buf(malloc(prev_len));
	prev.SerializeToArray(prev_buf.ptr, prev_len);
	
	//Serialize updated buffer
	int next_len = next.ByteSize();
	ScopeFree next_buf(malloc(next_len));
	next.SerializeToArray(next_buf.ptr, next_len);

	while(true)
	{
		//Start transaction
		if(!tchdbtranbegin(login_db))
		{
			return false;
		}
		
		//Retrieve current pointer
		int len;
		ScopeFree buf(tchdbget(login_db, (const void*)user_name.c_str(), user_name.size(), &len));
		if(buf.ptr == NULL ||
			prev_len != len)
		{
			tchdbtranabort(login_db);
			return false;
		}
		
		//Check that the buffer has not changed
		for(int i=0; i<len; ++i)
		{
			if(((uint8_t*)prev_buf.ptr)[i] != ((uint8_t*)buf.ptr)[i])
			{
				tchdbtranabort(login_db);
				return false;
			}
		}
		
		//Try writing updated record
		if(!tchdbput(login_db, 
			(const void*)user_name.c_str(), user_name.size(), 
			next_buf.ptr, next_len))
		{
			tchdbtranabort(login_db);
			continue;
		}
		
		//Commit transaction
		if(tchdbtrancommit(login_db))
			return true;
	}
}


