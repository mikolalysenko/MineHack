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
#include "httpserver.h"
#include "misc.h"

using namespace std;
using namespace Game;

bool app_running = false;

//The global config file
Config* config;

//The HttpServer
HttpServer* server;

//Handles an http event
bool callback(HttpEvent* event)
{
	Network::ServerPacket response_packet;
	
	response_packet.set_test_number(42);
	
	printf("got input from client: %d\n", event->client_packet->test_packet().test());
	
	event->reply(response_packet);

	delete event;
	return true;
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
	if(!server->start())
	{
		server->stop();
		printf("Failed to start server\n");
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
	server->stop();
	
	app_running = false;
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
	uint8_t test_buffer[] = { 18, 5, 8, 181, 149, 149, 48 };
	
	int sz;
	uint8_t* buf = (uint8_t*)tcdeflate((const char*)test_buffer, sizeof(test_buffer), &sz);

	printf("deflate stream, sz = %d, data = ", sz);
	for(int i=0; i<sz; ++i)
	{
		printf("%d,", buf[i]);
	}
	printf("\n");


	//Verify protocol buffers are working correctly
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	//Randomize timer
	srand(time(NULL));

	printf("Allocating objects\n");
	auto G1 = ScopeDelete<Config>(config = new Config("data/config.tc"));
	auto G2 = ScopeDelete<HttpServer>(server = new HttpServer(config, callback));

	init_app();
	console_loop();
	shutdown_app();
	
	//Kill protocol buffer library
	google::protobuf::ShutdownProtobufLibrary();

	return 0;
}

