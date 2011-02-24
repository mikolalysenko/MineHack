#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <sys/types.h>
#include <sys/socket.h>

#include <pthread.h>
#include <time>

#include <string>
#include <map>
#include <vector>

#include <cstdint>

#include "constants.h"
#include "network.pb.h"

namespace Server {

	typedef bool (*http_func)(HttpEvent*);
	
	//An HTTP client event packet
	struct HttpEvent
	{
		Network::ClientPacket client_packet;
		void reply(google::protobuf::Message& message);
		
	private:
		int socket;
	};

	//The HttpServer
	struct HttpServer
	{
		//Creates the HttpServer
		HttpServer(std::string const& wwwroot, http_func handler);
		~HttpServer();
		
		//Server start/stop functions
		void start();
		void stop();
		
		//Resets the cache for the server
		void reload_files();
		
	private:
		
		//If set, server is running
		volatile bool running;
		
		http_func callback;
		int epollfd;
		
		//The worker pool
		pthread_t workers[NUM_HTTP_WORKERS];
		
		//Cached http responses
		TCMAP* cached_response;
	};
};

#endif
