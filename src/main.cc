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
void reply_error(HttpEvent* event, string const& message)
{
	Network::ServerPacket response;
	response.mutable_error_response()->set_error_message(message);
	event->reply(response);
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
void handle_login(HttpEvent* event, Network::LoginRequest const& login_req)
{
	DEBUG_PRINTF("Got login request\n");

	if(!login_req.has_user_name())
	{
		reply_error(event, "Missing user name");
		return;
	}
	else if(!login_req.has_password_hash())
	{
		reply_error(event, "Missing password hash");
		return;
	}
	else if(!login_req.has_action())
	{
		reply_error(event, "Missing login action");
		return;
	}
	
	DEBUG_PRINTF("User name = %s, password hash = %s, action = %d\n",
		login_req.user_name().c_str(),
		login_req.password_hash().c_str(),
		login_req.action());
	
	Network::ServerPacket packet;
	Network::LoginResponse *response = packet.mutable_login_response();
	
	switch(login_req.action())
	{
	case Network::LoginRequest_LoginAction_Login:
	{
		Login::Account account;
		if(!login_db->get_account(login_req.user_name(), login_req.password_hash(), account))
		{
			reply_error(event, "Invalid user name/password");
			return;
		}
		
		response->set_success(true);
		response->mutable_character_names()->CopyFrom(account.character_names());
	}
	break;
	
	case Network::LoginRequest_LoginAction_CreateAccount:
	{
		if(!valid_user_name(login_req.user_name()))
		{
			reply_error(event, "Invalid user name");
			return;
		}
		
		if(!login_db->create_account(login_req.user_name(), login_req.password_hash()))
		{
			reply_error(event, "User name taken");
			return;
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
			reply_error(event, "Invalid user name/password");
			return;
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
			reply_error(event, "Bad character name");
			return;
		}
	
		Login::Account account;
		if(!login_db->get_account(login_req.user_name(), login_req.password_hash(), account))
		{
			reply_error(event, "Invalid user name/password");
			return;
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
			reply_error(event, "Could not update account");
			return;
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
			reply_error(event, "Missing character name");
			return;
		}
		
		Login::Account account;
		if(!login_db->get_account(login_req.user_name(), login_req.password_hash(), account))
		{
			reply_error(event, "Invalid user name/password");
			return;
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
			reply_error(event, "Character does not exist");
			return;
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
			reply_error(event, "Could not update account");
			return;
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
			reply_error(event, "Missing character name");
			return;
		}
	
		Login::Account account;
		if(!login_db->get_account(login_req.user_name(), login_req.password_hash(), account))
		{
			reply_error(event, "Invalid user name/password");
			return;
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
			reply_error(event, "Character does not exist");
			return;
		}

		//FIXME: Try joining game world
		
		response->set_success(true);
	}
	break;
	}
	
	//Reply with response packet
	event->reply(packet);
}

//Handles an http event
void callback(HttpEvent* event)
{
	if(event->client_packet->has_login_packet())
	{
		handle_login(event, event->client_packet->login_packet());
		delete event;
	}
	else
	{
		//Client is fucking with us.  Send them a nasty error message.
		event->error();
		delete event;
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
void sync()
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
			sync();
		}
		else if(command == "stop")
		{
			shutdown_app();
		}
		else if(command == "start")
		{
			init_app();
		}
		else if(command == "restart")
		{
			shutdown_app();
			init_app();
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

