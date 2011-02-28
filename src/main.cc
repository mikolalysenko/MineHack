#include <pthread.h>
#include <time.h>

#include <cstdint>
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

using namespace std;
using namespace Game;

bool app_running = false;

//The global config file
Config* config;

//Login database
LoginDB* login_db;

//The HttpServer
HttpServer* server;

//Sends a nice error message to the client
void reply_error(HttpEvent* event, string const& message)
{
	Network::ServerPacket response;
	response.mutable_error_response()->set_error_message(message);
	event->reply(response);
}

//Handles a login request
void handle_login(HttpEvent* event, Network::LoginRequest const& login_req)
{
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
	
	Network::ServerPacket packet;
	Network::LoginResponse *response = packet.mutable_login_response();
	
	switch(login_req.action())
	{
	case Network::LoginRequest_LoginAction_Login:
	{
		Login::Account account;
		if(!login_db->get_account(login_req.user_name(), account) ||
			account.password_hash() != login_req.password_hash())
		{
			reply_error(event, "Invalid user name/password");
			return;
		}
		
		response->set_success(true);
		
		//FIXME: Create session here

		response->set_success(true);
		response->set_session_id(0);
	}
	break;
	
	case Network::LoginRequest_LoginAction_Create:
	{
		//FIXME: Validate user name here
		if(!login_db->create_account(login_req.user_name(), login_req.password_hash()))
		{
			reply_error(event, "User name taken");
			return;
		}
		
		
		Login::Account account;
		login_db->get_account(login_req.user_name(), account);
		
		//FIXME: Create session here
		
		response->set_success(true);
		response->set_session_id(0);
	}
	break;

	case Network::LoginRequest_LoginAction_Delete:
	{
		Login::Account account;
		if(!login_db->get_account(login_req.user_name(), account) ||
			account.password_hash() != login_req.password_hash()  )
		{
			reply_error(event, "Invalid user name/password");
			return;
		}
		
		login_db->delete_account(login_req.user_name());
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
	
	printf("Starting http server\n");
	if(!server->start())
	{
		login_db->stop();
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
	auto GL = ScopeDelete<LoginDB>(login_db = new LoginDB(config));
	auto GS = ScopeDelete<HttpServer>(server = new HttpServer(config, callback));

	init_app();
	console_loop();
	shutdown_app();
	
	//Kill protocol buffer library
	google::protobuf::ShutdownProtobufLibrary();

	return 0;
}

