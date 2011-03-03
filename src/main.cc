#include <pthread.h>
#include <time.h>

#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <tcutil.h>

#include "network.pb.h"
#include "login.pb.h"

#include "constants.h"
#include "config.h"
#include "login.h"
#include "httpserver.h"
#include "misc.h"
#include "world.h"

using namespace std;
using namespace Game;


//Uncomment this line to get dense logging for the web server
#define APP_DEBUG 1

#ifndef APP_DEBUG
#define DEBUG_PRINTF(...)
#else
#define DEBUG_PRINTF(...)  fprintf(stderr,__VA_ARGS__)
#endif


bool app_running = false;

Config* 	config;
LoginDB* 	login_db;
HttpServer* server;
World*		world;


//Sends a nice error message to the client
Network::ServerPacket* error_response(string const& message)
{
	auto response = new Network::ServerPacket();
	response->mutable_error_response()->set_error_message(message);
	return response;
}

//Validates a user name
bool valid_user_name(string const& user_name)
{
	//FIXME: Add stuff here to prevent client side javascript injection
	return true;
}

bool valid_character_name(string const& character_name)
{
	//FIXME: Do validation stuff here
	return true;
}

//Handles a login request
Network::ServerPacket* handle_login(Network::LoginRequest const& login_req)
{
	DEBUG_PRINTF("Got login request\n");

	if(!login_req.has_user_name())
	{
		return error_response("Missing user name");
	}
	else if(!login_req.has_password_hash())
	{
		return error_response("Missing password hash");
	}
	else if(!login_req.has_action())
	{
		return error_response("Missing login action");
	}
	
	DEBUG_PRINTF("User name = %s, password hash = %s, action = %d\n",
		login_req.user_name().c_str(),
		login_req.password_hash().c_str(),
		login_req.action());
	
	Network::ServerPacket packet;
	auto response = packet.mutable_login_response();
	
	switch(login_req.action())
	{
	case Network::LoginRequest_LoginAction_Login:
	{
		Login::Account account;
		if(!login_db->get_account(login_req.user_name(), login_req.password_hash(), account))
		{
			return error_response("Invalid user name/password");
		}
		
		response->set_success(true);
		response->mutable_character_names()->CopyFrom(account.character_names());
	}
	break;
	
	case Network::LoginRequest_LoginAction_CreateAccount:
	{
		if(!valid_user_name(login_req.user_name()))
		{
			return error_response("Invalid user name");
		}
		
		if(!login_db->create_account(login_req.user_name(), login_req.password_hash()))
		{
			return error_response("User name taken");
		}
		
		response->set_success(true);
		response->mutable_character_names()->Clear();
	}
	break;

	case Network::LoginRequest_LoginAction_DeleteAccount:
	{
		Login::Account account;
		if(!login_db->get_account(login_req.user_name(), login_req.password_hash(), account))
		{
			return error_response("Invalid user name/password");
		}
		
		login_db->delete_account(login_req.user_name());
		response->set_success(true);
	}
	break;
	
	case Network::LoginRequest_LoginAction_CreateCharacter:
	{
		if( !login_req.has_character_name() ||
			!valid_character_name(login_req.character_name()) )
		{
			return error_response("Bad character name");
		}
	
		Login::Account account;
		if(!login_db->get_account(login_req.user_name(), login_req.password_hash(), account))
		{
			return error_response("Invalid user name/password");
		}

		//FIXME: Try adding character to game world
		
		//Add character
		Login::Account next_account;
		next_account.CopyFrom(account);
		next_account.add_character_names(login_req.character_name());
		
		//Update database
		if(!login_db->update_account(login_req.user_name(), account, next_account))
		{
			//FIXME: Remove character from game world
			return error_response("Could not update account");
		}
		
		//Set response values
		response->set_success(true);
		response->mutable_character_names()->CopyFrom(next_account.character_names());
	}
	break;
	
	case Network::LoginRequest_LoginAction_DeleteCharacter:
	{
		if(!login_req.has_character_name())
		{
			return error_response("Missing character name");
		}
		
		Login::Account account;
		if(!login_db->get_account(login_req.user_name(), login_req.password_hash(), account))
		{
			return error_response("Invalid user name/password");
		}

		int idx = -1, sz = account.character_names_size();
		for(int i=0; i<sz; ++i)
		{
			if(account.character_names(i) == login_req.character_name())
			{
				idx = i;
				break;
			}
		}
		
		if(idx == -1)
		{
			return error_response("Character does not exist");
		}
		
		//FIXME: Try deleting from world
		
		//Remove character
		Login::Account next_account;
		next_account.CopyFrom(account);
		next_account.set_character_names(idx, account.character_names(sz-1));
		next_account.mutable_character_names()->RemoveLast();
		
		//Update database
		if(!login_db->update_account(login_req.user_name(), account, next_account))
		{
			return error_response("Could not update account");
		}
		
		//Set response values
		response->set_success(true);
		response->mutable_character_names()->CopyFrom(next_account.character_names());
	}
	break;

	case Network::LoginRequest_LoginAction_Join:
	{
		if(!login_req.has_character_name())
		{
			return error_response("Missing character name");
		}
	
		Login::Account account;
		if(!login_db->get_account(login_req.user_name(), login_req.password_hash(), account))
		{
			return error_response("Invalid user name/password");
		}
		
		int idx = -1, sz = account.character_names_size();
		for(int i=0; i<sz; ++i)
		{
			if(account.character_names(i) == login_req.character_name())
			{
				idx = i;
				break;
			}
		}
		
		if(idx == -1)
		{
			return error_response("Character does not exist");
		}

		//FIXME: Allocate session id for character and return it
		
		response->set_success(true);
	}
	break;
	}
	
	return new Network::ServerPacket(packet);
}

//Handles an http event
Network::ServerPacket* callback(Network::ClientPacket* client_packet)
{
	if(client_packet->has_login_packet())
	{
		return handle_login(client_packet->login_packet());
	}
	else
	{
		return NULL;
	}
}

//Server initialization
void init_app()
{
	if(app_running)
	{
		printf("App already started\n");
		return;
	}

	printf("Starting app\n");
	
	
	printf("Starting login database\n");
	if(!login_db->start())
	{
		login_db->stop();
		printf("Failed to start login database\n");
		return;
	}

	printf("Starting world instance\n");
	if(!world->start())
	{
		world->stop();
		printf("Failed to start world instance");
		return;
	}
	
	printf("Starting http server\n");
	if(!server->start())
	{
		login_db->stop();
		world->stop();
		server->stop();
		printf("Failed to start http server\n");
		return;
	}
	
	app_running = true;
}


//Kills server
void shutdown_app()
{
	if(!app_running)
	{
		printf("App already stopped\n");
		return;
	}

	printf("Stopping app\n");

	printf("Stopping world instance\n");
	world->stop();
	
	printf("Stopping http server\n");
	server->stop();
	
	printf("Stopping login database\n");
	login_db->stop();
	
	
	app_running = false;
}

//Saves the state of the entire game to the database
void resync()
{
	if(!app_running)
	{
		printf("App is not running\n");
		return;
	}
	
	printf("Synchronizing login\n");
	login_db->sync();
	
	printf("Synchronizing world state\n");
	world->sync();
}

//This loop runs in the main thread.  Implements the admin console
void console_loop()
{
	while(true)
	{
		string command;
		
		cin >> command;
		
		if( command == "q" || command == "quit" )
		{
			break;
		}
		else if(command == "r" || command == "recache")
		{
			if(app_running)
			{
				printf("Recaching wwwroot\n");
				server->recache();
			}
			else
			{
				printf("App not running");
			}
		}
		else if(command == "s" || command == "sync")
		{
			resync();
		}
		else if(command == "print")
		{
			string a;
			cin >> a;
			cout << config->readString(a) << endl;
		}
		else if(command == "set")
		{
			string a, b;
			cin >> a >> b;
			config->storeString(a, b);
		}
		else if(command == "reset_config")
		{
			printf("Resetting configuration file to defaults\n");
			config->resetDefaults();
		}
		else if(command == "help")
		{
			printf("Read source code for documentation\n");
		}
	}
}

//Program start point
int main(int argc, char** argv)
{
	//Verify protocol buffers are working correctly
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	//Randomize timer
	srand(time(NULL));

	printf("Allocating objects\n");
	auto GC = ScopeDelete<Config>(config = new Config("data/config.tc"));
	auto GW = ScopeDelete<World>(world = new World(config));
	auto GL = ScopeDelete<LoginDB>(login_db = new LoginDB(config));
	auto GS = ScopeDelete<HttpServer>(server = new HttpServer(config, callback));

	init_app();
	console_loop();
	shutdown_app();
	
	//Kill protocol buffer library
	google::protobuf::ShutdownProtobufLibrary();

	return 0;
}

