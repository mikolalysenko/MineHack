#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <pthread.h>

#include <string>
#include <map>
#include <vector>

#include <cstdint>

#include "constants.h"
#include "config.h"
#include "network.pb.h"

namespace Game {


	//The network event wrapper
	enum HttpEventType
	{
		Connect,
		Send,
		Recv
	};
	
	//An HTTP client event packet
	struct HttpEvent
	{
		HttpEventType type;
		Network::ClientPacket client_packet;
		
		void reply(google::protobuf::Message& message);
	};

	//The callback function type
	typedef bool (*http_func)(HttpEvent*);

	//The HttpServer
	struct HttpServer
	{
		//Creates the HttpServer
		HttpServer(Config* cfg, http_func handler);
		~HttpServer();
		
		bool start();
		void stop();
		
	private:

		//Configuration stuff
		Config* config;
		http_func callback;
		
		//Server parameters
		volatile bool running;
		int epollfd, listen_socket, living_threads;
		HttpEvent* listen_event;
		
		//Worker stuff
		pthread_t workers[NUM_HTTP_WORKERS];
		static void* worker_start(void*);
		void worker_loop();
		
		//Cache functions
		TCMAP*	cached_response;
		void init_cache();
		const char* get_cached_response(const char* filename, int filename_len, int* cache_len);
		void clear_cache();
	};
};

#endif
