#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <pthread.h>
#include <netinet/in.h>

#include <string>
#include <map>
#include <vector>

#include <stdint.h>

#include <tbb/concurrent_queue.h>
#include <tbb/concurrent_hash_map.h>

#include "constants.h"
#include "config.h"
#include "httpparser.h"

#include "network.pb.h"

namespace Game {

	enum SocketState
	{
		SocketState_Listening,
		SocketState_WaitForHeader,
		SocketState_CachedReply,
		SocketState_PostRecv,
		SocketState_PostReply,
	};

	//Either send or recv side of a socket
	struct SocketSide
	{
		int size, pending;
		char *buf_start, *buf_cur;
		
		SocketSide() : size(0), pending(0), buf_start(NULL), buf_cur(NULL) { }
		
		~SocketSide() 
		{
			if(buf_start != NULL)
				free(buf_start);
		}
	};

	//The internal HTTP client event packet structure
	struct Socket
	{
		Socket(int fd, bool listener);
		~Socket();
	
		//Socket descriptors
		int socket_fd;
		sockaddr_storage addr;
		
		//Internal socket state
		SocketState state;
		SocketSide inp;
		SocketSide outp;
		
		//Special case for cached get requests
		tbb::concurrent_hash_map<std::string, HttpResponse>::const_accessor cached_response;
	};
	
	//The callback function type
	typedef Network::ServerPacket* (*command_func)(Network::ClientPacket*);

	//The HttpServer
	class HttpServer
	{
	public:
		//Creates the HttpServer
		HttpServer(Config* cfg, command_func command_handler);
		
		//Start the server
		bool start();
		void stop();
		
		//Recaches all static http content
		void recache();		
		
	private:

		//Configuration stuff
		Config*			config;
		command_func	command_callback;

		//Basic server stuff
		volatile bool running;
		int living_threads;
		int epollfd;
		
		//Socket set management
		tbb::concurrent_hash_map<int, Socket*> socket_set;
		Socket* create_socket(int fd, bool listener, sockaddr_storage* addr);
		bool notify_socket(Socket*);
		void dispose_socket(Socket*);
		void cleanup_sockets();
		
		//Event handlers
		void process_accept(Socket*);
		void process_header(Socket*);
		void process_reply_cached(Socket*);
		void process_post_recv(Socket*);
		void process_post_send(Socket*);
		
		//Worker stuff
		pthread_t workers[NUM_HTTP_WORKERS];
		static void* worker_start(void*);
		void worker_loop();
		
		//Cache functions
		tbb::concurrent_hash_map<std::string, HttpResponse >	cached_responses;
		bool get_response(std::string const& request, tbb::concurrent_hash_map<std::string, HttpResponse>::const_accessor& acc);
		void cleanup_cache();
	};
};

#endif
