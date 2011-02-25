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

	enum HttpEventState
	{
		Listening,
		WaitForHeader,
		Receiving,
		Processing,
		Sending
	};

	//An HTTP client event packet
	struct HttpEvent
	{
		//Don't ever call these
		HttpEvent();
		~HttpEvent();
	
		//The internal socket file descriptor
		int socket_fd;
		
		//State for the event
		HttpEventState state;
		
		//Memory buffers
		bool no_delete;
		int buf_size, content_length, pending_bytes;
		char *buf_start, *buf_cur, *content_ptr;
		
		//Try parsing the request
		int try_parse(char** request, int* request_len, char** content_start, int* content_length);
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

		//Basic server stuff
		volatile bool running;
		int living_threads;
		
		//Event stuff
		int epollfd;
		pthread_spinlock_t event_map_lock;
		TCMAP* event_map;
		HttpEvent* create_event(int fd, bool add_epoll = true);
		void dispose_event(HttpEvent* event);
		void cleanup_events();
		
		//Event handlers
		void accept_event(HttpEvent* event);
		void process_header(HttpEvent* event);
		void process_cached_request(HttpEvent* event, char* req, int req_len);
		void process_recv(HttpEvent* event);
		void process_send(HttpEvent* event);
		
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
