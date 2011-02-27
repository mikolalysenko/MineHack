#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <pthread.h>
#include <netinet/in.h>

#include <string>
#include <map>
#include <vector>

#include <cstdint>

#include "constants.h"
#include "config.h"
#include "network.pb.h"

namespace Game {

	class HttpServer;

	enum SocketEventState
	{
		Listening,
		WaitForHeader,
		Receiving,
		Processing,
		Sending
	};

	//The internal HTTP client event packet structure
	//Generally speaking, this is only used inside the http server.  Users interact with
	// HttpEvent
	struct SocketEvent
	{
		SocketEvent(int fd, bool listener);
		~SocketEvent();
	
		//The internal socket file descriptor
		int socket_fd;
		
		//Socket address
		sockaddr_storage addr;
		
		//State for the event
		SocketEventState state;
		
		//Amount of pending bytes for current send/recv
		int pending_bytes;
		
		//Pointer to packet contents		
		int content_length;
		char* content_ptr;
		
		//Recv buffer stuff
		int recv_buf_size;
		char *recv_buf_start, *recv_buf_cur;
		
		//Send buffer stuff
		bool free_send_buffer;
		char *send_buf_start, *send_buf_cur;
		
		//Try parsing the request
		int try_parse(char** request, int* request_len);
	};
	
	//The external interface to the HttpEvent server
	struct HttpEvent
	{
		HttpEvent(Network::ClientPacket* packet, SocketEvent* s_event, HttpServer* server);
		~HttpEvent();
		
		//Sends a reply
		bool reply(void* buf, int len, bool release);
		bool reply(Network::ServerPacket& message);
		
		//Sends a 403 forbidden
		bool error();
		
		//The client packet data
		Network::ClientPacket* client_packet;
		
		//Used for implementing a linked list
		HttpEvent* next;
		
	private:
		SocketEvent*	socket_event;
		HttpServer*		server;
	};
	
	
	//The callback function type
	typedef void (*http_func)(HttpEvent*);

	//The HttpServer
	struct HttpServer
	{
		//Creates the HttpServer
		HttpServer(Config* cfg, http_func handler);
		~HttpServer();
		
		bool start();
		void stop();
		
		//Recaches all pages
		void recache();
		
		//DO NOT CALL THESE METHODS.  They are used internally by the HttpEvent class to manage sockets.
		//C++'s stupid scoping rules make it very difficult to let HttpEvent access these things in some
		//non-obfuscated, efficient way.
		void dispose_event(SocketEvent* event);
		bool initiate_send(SocketEvent* event, void* buf, int len, bool release);
		
	private:

		//Configuration stuff
		Config* config;
		http_func callback;

		//Basic server stuff
		volatile bool running;
		int living_threads;
		
		//Event stuff
		int epollfd;
		pthread_spinlock_t event_map_lock;
		TCMAP* event_map;
		SocketEvent* create_event(int fd, bool listener = false);
		void cleanup_events();
		
		//Event handlers
		void accept_event(SocketEvent* event);
		void process_header(SocketEvent* event, bool do_recv = true);
		void process_cached_request(SocketEvent* event, char* req, int req_len);
		void process_command(SocketEvent* event);
		void process_recv(SocketEvent* event);
		void process_send(SocketEvent* event);
		
		//Worker stuff
		pthread_t workers[NUM_HTTP_WORKERS];
		static void* worker_start(void*);
		void worker_loop();
		
		//Cache functions
		pthread_spinlock_t cache_lock;
		TCMAP*	cached_response;
		TCLIST*	old_cache;
		const char* get_cached_response(const char* filename, int filename_len, int* cache_len);
	};
};

#endif
