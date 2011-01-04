#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <cctype>

#include <unistd.h>
#include <pthread.h>

#include "misc.h"
#include "login.h"

using namespace std;

namespace Login
{
const int PASSWORD_HASH_LEN = 64;
const int USERNAME_MAX_LEN = 20;
const int USERNAME_MIN_LEN = 3;

//User account data
struct LoginData
{
	string	user_name;
	string password_hash;

	//Other account wide data goes here
};

//Lock for the login dictionary
static pthread_rwlock_t login_lock = PTHREAD_RWLOCK_INITIALIZER;

static map<string, LoginData>	login_table;

typedef map<string, LoginData>::iterator login_iter;


//Initializes the login table
void init_login()
{
	ifstream users("data/login.dat");
	
	while(users.good())
	{
		LoginData nlogin;
		users >> nlogin.user_name >> nlogin.password_hash;
		login_table[nlogin.user_name] = nlogin;
	}
}

//Check points all the user login data
void checkpoint_login()
{
	{	ofstream data("data/login.tmp.dat");
		ReadLock L(&login_lock);
	
		for(auto it = login_table.begin();
			it != login_table.end();
			++it)
		{
			LoginData login = (*it).second;
			data << login.user_name << ' ' << login.password_hash << endl;
		}
	}
	
	unlink("data/login.dat");
	link("data/login.tmp.dat", "data/login.dat");
	unlink("data/login.tmp.dat");
}

//Verifies a user name/password combo
bool verify_user_name(const string& name, const string& password_hash)
{
	bool valid = false;

	{	ReadLock L(&login_lock);

		login_iter row = login_table.find(name);
		if(row != login_table.end())
		{
			valid = ((*row).second.password_hash == password_hash);
		}
	}
	
	return valid;
}


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

//Creates an account
bool create_account(const string& name, const string& password_hash)
{
	//Validate name and password
	if(!(is_valid_username(name) && is_valid_password_hash(password_hash)))
	{
		return false;
	}
	
	//Create login record
	LoginData login;
	login.user_name = name;
	login.password_hash = password_hash;

	//Insert user into table
	bool success = true;
	
	{	WriteLock L(&login_lock);
	
		if(login_table.find(name) == login_table.end())
		{	login_table[name] = login;
		}
		else
		{	success = false;
		}	
	}
	    
    return success;
}


};


