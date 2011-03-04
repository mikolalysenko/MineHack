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
#include "ws_queue.h"

#include "network.pb.h"

namespace Game {

	class HttpServer;
	class Socket;

	//The user interface for a web socket.  Queues up send/recv packets
	class WebSocket
	{
	public:
		WebSocket(HttpServer*, WebSocketQueue_Impl*, WebSocketQueue_Impl*, int, Socket*);
		~WebSocket();
	
		bool send_packet(Network::ServerPacket*);
		bool recv_packet(Network::ClientPacket*&);
		
	private:
	
		WebSocketQueue	send_q, recv_q;
		
		//Used for notifying the send socket
		HttpServer* server;
		int send_fd;
		Socket* send_socket;
	};


	enum SocketState
	{
		SocketState_Listening			= 1,
		SocketState_WaitForHeader		= 2,
		SocketState_CachedReply			= 3,
		SocketState_PostRecv			= 4,
		SocketState_PostReply			= 5,
		SocketState_WebSocketHandshake	= 6,
		SocketState_WebSocketPoll		= 7,
		SocketState_WebSocketSend		= 8,
	};

	//Either send or recv side of a socket
	struct SocketSide
	{
		int size, pending;
		char *buf_start, *buf_cur, *frame_start;
		
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
		
		//Web socket message queue
		WebSocketQueue	message_queue;
		
		//Request header information
		HttpRequest request;
		
		//Special case for cached get requests
		tbb::concurrent_hash_map<std::string, HttpResponse>::const_accessor cached_response;
	};
	
	//Post callback function type
	typedef Network::ServerPacket* (*command_func)(HttpRequest const& request, Network::ClientPacket*);

	//Web socket accept function type
	typedef bool (*websocket_func)(HttpRequest const&, WebSocket* ws);

	//The HttpServer
	class HttpServer
	{		
	public:
		//Creates the HttpServer
		HttpServer(Config* cfg, 
			command_func command_handler,
			websocket_func websocket_handler);
		
		//Start the server
		bool start();
		void stop();
		
		//Recaches all static http content
		void recache();
		
		
	private:

		//Configuration stuff
		Config*			config;
		command_func	command_callback;
		websocket_func	websocket_callback;

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

		//Web socket stuff
		void initialize_websocket(Socket*);
		void handle_websocket_recv(Socket*);
		void handle_websocket_send(Socket*);
		void dispose_websocket(Socket*, Socket*);
		
		//Event handlers
		void process_accept(Socket*);
		void process_header(Socket*);
		void process_reply_cached(Socket*);
		void process_post(Socket*);
		void process_recv(Socket*);
		void process_send(Socket*);
		
		//Worker stuff
		pthread_t workers[NUM_HTTP_WORKERS];
		static void* worker_start(void*);
		void worker_loop();
		
		//Cache functions
		tbb::concurrent_hash_map<std::string, HttpResponse >	cached_responses;
		bool get_cached_response(std::string const& request, tbb::concurrent_hash_map<std::string, HttpResponse>::const_accessor& acc);
		void cleanup_cache();
		
		
		friend void notify_websocket_send(HttpServer* server, int send_fd, Socket* socket);

	};
};

#endif
