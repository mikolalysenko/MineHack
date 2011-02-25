#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <map>
#include <vector>
#include <string>

#include <tcutil.h>

#include "constants.h"
#include "config.h"
#include "httpserver.h"
#include "misc.h"

#include "network.pb.h"

using namespace std;
using namespace Game;

const char DEFAULT_ERROR_RESPONSE[] = 
	"HTTP/1.1 403 Forbidden\n\n";

const char DEFAULT_HTTP_HEADER[] =  
	"HTTP/1.1 200 OK\n"
	"Cache-Control: max-age=300\n"
	"Content-Encoding: gzip\n"
	"Content-Length: %d\n\n";


//Initialize SocketEvent fields
SocketEvent::SocketEvent(int fd, bool listener) : 
	socket_fd(fd), 
	free_send_buffer(false),
	send_buf_start(NULL),
	send_buf_cur(NULL)
{
	if(listener)
	{
		state = Listening;
		recv_buf_cur = recv_buf_start = NULL;
		recv_buf_size = 0;
	}
	else
	{
		state = WaitForHeader;
		recv_buf_size = RECV_BUFFER_SIZE;
		recv_buf_start = (char*)malloc(RECV_BUFFER_SIZE);
		recv_buf_cur = recv_buf_start;
	}
}

//Destroy http event
SocketEvent::~SocketEvent()
{
	if(socket_fd != -1)
	{
		printf("Closing socket: %d\n", socket_fd);
		close(socket_fd);
	}
	
	if(recv_buf_start != NULL)
	{
		free(recv_buf_start);
	}
	
	if(free_send_buffer)
	{
		free(send_buf_start);
	}
}


// HTTP Header parsing philosophy:
//
// Needs to accept output from our supported platforms and do it fast, and reject outright malicious 
// code.  Any extra cruft is ignored.
//
// Return code:
//   1 = success
//   0 = error
//  -1 = incomplete
//
int SocketEvent::try_parse(char** request, int* request_len, char** content_start, int* content_length)
{
	//FIXME: Put a cap on the end_ptr length to limit ddos attacks
	char *ptr     = (char*)recv_buf_start,
		 *end_ptr = (char*)recv_buf_cur,
		 c;
		  
	//Check for no data case
	if(ptr == end_ptr)
		return -1;
		  
	//Eat leading whitespace
	c = *ptr;
	while(
		(c == ' ' || c == '\r' || c == 'n' || c == '\t') &&
		ptr < end_ptr)
	{
		c = *(ptr++);
	}
		  
	int len = (int)(end_ptr - ptr);
	if(len < 12)
		return -1;
	
	//Check for get or post
	bool post = false;
	if(ptr[0] == 'G')
	{
		ptr += 4;
	}
	else if(ptr[0] == 'P')
	{
		ptr += 5;
		post = true;
	}
	else
	{
		printf("Unkown HTTP request type\n");
		return 0;
	}
	
	//Skip leading slash
	if(*ptr == '/')
	{
		ptr++;	
	}
	
	//For get stuff, we are done
	if(!post)
	{
		//Handle get request
		*content_length = 0;
		*request = ptr;
	
		//Read to end of request
		while(ptr < end_ptr)
		{
			c = *(ptr++);
			if(c == ' ' || c == '\0')
				break;
		}
	
		//Set end of request
		if(ptr == end_ptr)
			return -1;
		*(ptr-1) = '\0';
		*request_len = (int)(ptr - 1 - *request);
		
		return 1;
	}
	
	//Seek to content length field
	while(ptr < end_ptr)
	{
		c = *(ptr++);
		if(c == 'C')
		{
			//Check for field match
			const char ONTENT_LEN[] = "ontent-Length:";
			if(end_ptr - ptr < sizeof(ONTENT_LEN))
				return -1;
			
			if(strcmp(ptr, ONTENT_LEN) == 0)
			{
				ptr += sizeof(ONTENT_LEN);
				break;
			}
		}
	}
	if(ptr == end_ptr)
		return -1;
	
	//Seek to end of line
	char* content_str = ptr;
	while(ptr < end_ptr)
	{
		c = *(ptr++);
		if(c == '\n' || c == '\r' || c == '\0')
			break;
	}
	
	if(ptr == end_ptr)
		return -1;
	*(--ptr) = '\0';	
	*content_length = atoi(content_str);
	
	//Error: must specify content length > 0
	if(*content_length == 0)
		return 0;
	
	//Restore last character
	*ptr = c;
	
	//Search for two consecutive eol
	int eol_cnt = 0;
	while(ptr < end_ptr)
	{
		c = *(ptr++);
		
		if(c == '\n')
		{
			if(++eol_cnt == 2)
				break;
		}
		else if(c == '\r')
		{
		}
		else
		{
			eol_cnt = 0;
		}
	}
	
	if(eol_cnt == 2)
	{
		*content_start = ptr;
		return 1;
	}
	
	return -1;
}


HttpServer::HttpServer(Config* cfg, http_func cback) :
	config(cfg), 
	callback(cback), 
	cached_response(tcmapnew()),
	event_map(tcmapnew())
{
	pthread_spin_init(&event_map_lock, PTHREAD_PROCESS_PRIVATE);
}

HttpServer::~HttpServer()
{
	clear_cache();
	tcmapdel(cached_response);
	tcmapdel(event_map);
}


//Recursively enumerates all files in a directory
int list_files(string dir, vector<string> &files)
{
	DIR* dp;
	dirent* dirp;
	
	if((dp = opendir(dir.c_str())) == NULL)
	{
		return errno;
	}

	while ((dirp = readdir(dp)) != NULL)
	{
		string base = dirp->d_name;
		
		if(base == "." || base == "..")
			continue;
	
		string fname;
		fname.append(dir);
		fname.append("/");
		fname.append(base);
	
		if(list_files(fname, files) != 0)
			files.push_back(fname);
	}
	
	closedir(dp);
	return 0;
}

//Reads a whole file into memory
void* read_file(string filename, int* length)
{
	FILE* fp = fopen(filename.c_str(), "rb");
	if(fp == NULL)
		return NULL;
	
	//Get length
	fseek(fp, 0L, SEEK_END);
	*length = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	
	//Read whole file
	void* result = malloc(*length);
	int len = fread(result, 1, *length, fp);
	fclose(fp);
	
	if(len < *length)
	{
		free(result);
		*length = 0;
		return NULL;
	}
	
	return result;
}

//Initialize the http response cache
void HttpServer::init_cache()
{

	printf("Initializing cache\n");
	
	vector<string> files;
	list_files(config->readString("wwwroot"), files);

	for(int i=0; i<files.size(); ++i)
	{
		//Read file into memory
		int raw_size;
		ScopeFree raw_data(read_file(files[i], &raw_size));
		
		if(raw_data.ptr == NULL)
		{
			printf("\tError reading file: %s\n", files[i].c_str());
			continue;
		}
		printf("\tCaching file: %s\n", files[i].c_str());
		
		//Compress
		int compressed_size;
		ScopeFree compressed_data((void*)tcgzipencode((char*)raw_data.ptr, raw_size, &compressed_size));
		
		//Build response
		ScopeFree response(malloc(compressed_size + sizeof(DEFAULT_HTTP_HEADER) + 10));
		int header_len = sprintf((char*)response.ptr, DEFAULT_HTTP_HEADER, compressed_size);
		int response_len = header_len + compressed_size;
		memcpy((char*)response.ptr+header_len, compressed_data.ptr, compressed_size);


		//Strip off leading path
		for(int j=0; j<files[i].size(); ++j)
		{
			if(files[i][j] == '/')
			{
				files[i] = files[i].substr(j+1, files[i].size());
				break;
			}
		}
		
		//Store in cache
		tcmapput(cached_response, files[i].c_str(), files[i].size(), response.ptr, response_len);
	}
}

//Clear out the cache
void HttpServer::clear_cache()
{
	tcmapclear(cached_response);
}

//Retrieves a cached response
const char* HttpServer::get_cached_response(const char* filename, int filename_len, int* response_len)
{
	const char* result = (const char*)tcmapget(cached_response, filename, filename_len, response_len);
	
	if(result == NULL)
	{
		result = DEFAULT_ERROR_RESPONSE;
		*response_len = sizeof(DEFAULT_ERROR_RESPONSE);
	}
	
	return result;
}

//Creates an http event
SocketEvent* HttpServer::create_event(int fd, bool listener)
{
	SocketEvent* result = new SocketEvent(fd, listener);
	result->socket_fd = fd;
	
	//Store pointer in map
	pthread_spin_lock(&event_map_lock);
	bool success = tcmapputkeep(event_map, &fd, sizeof(fd), &result, sizeof(result));
	pthread_spin_unlock(&event_map_lock);
	
	if(success)
	{
		if(listener)
		{
			epoll_event ev;
			ev.events = EPOLLIN | EPOLLPRI;
			ev.data.fd = fd;
			ev.data.ptr = result;

			if(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1)
			{
				perror("epoll_add");
				delete result;
				return NULL;
			}
		}
		else
		{
			//Set socket to non blocking
			int flags = fcntl(fd, F_GETFL, 0);
 			if (flags == -1)
        		flags = 0;
        	if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        	{
        		perror("fcntl");
        		delete result;
        		return NULL;
        	}
        	
        	//Add to epoll
			epoll_event ev;
			ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
			ev.data.fd = fd;
			ev.data.ptr = result;
			if(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1)
			{
				perror("epoll_ctl");
				delete result;
				return NULL;
			}
		}
		
		
		return result;
	}
	
	delete result;
	return NULL;
}

//Disposes of an http event
void HttpServer::dispose_event(SocketEvent* event)
{
	printf("Disposing socket\n");
	epoll_ctl(epollfd, EPOLL_CTL_DEL, event->socket_fd, NULL);	
	pthread_spin_lock(&event_map_lock);
	tcmapout(event_map, &event->socket_fd, sizeof(int));
	pthread_spin_unlock(&event_map_lock);
	delete event;
}

//Clean up all events in the server
// (This is always called after epoll has stopped)
void HttpServer::cleanup_events()
{
	TCLIST* events = tcmapvals(event_map);
	while(true)
	{
		int sz;
		void* ptr = tclistpop(events, &sz);
		if(ptr == NULL)
			break;
			
		SocketEvent* event = *((SocketEvent**)ptr);
		delete event;
		free(ptr);
	}
	tclistdel(events);
	
	tcmapclear(event_map);
}

//Start the server
bool HttpServer::start()
{
	printf("Starting http server...\n");
	
	epollfd = -1;
	living_threads = 0;
	init_cache();	
	
	//Create listen socket
	printf("Creating listen socket\n");
	int listen_socket = socket(PF_INET, SOCK_STREAM, 0);
	if(listen_socket == -1)
	{
		perror("socket");
		stop();
		return false;
	}

	//Bind socket address
	sockaddr_in addr;
	addr.sin_addr.s_addr	= INADDR_ANY;
	addr.sin_port 			= htons(config->readInt("listenport"));
	addr.sin_family			= AF_INET;
	
	int optval = 1;
	if(setsockopt(listen_socket, SOL_SOCKET,  SO_REUSEADDR, &optval, sizeof(optval)) == -1)
	{
		perror("setsockopt");
		stop();
		return false;
	}
	
	if(bind(listen_socket, (sockaddr *) (void*)&addr, sizeof(addr)) == -1)
	{
		perror("bind");
		stop();
		return false;
	}

	//Listen
	if (listen(listen_socket, LISTEN_BACKLOG) == -1) {
	    perror("listen()");
	    stop();
	    return false;
	}
	

	//Create epoll object
	printf("Creating epoll object\n");
	epollfd = epoll_create(1000);
	if(epollfd == -1)
	{
		perror("epoll_create");
		stop();
		return false;
	}

	//Add listen socket event
	auto listen_event = create_event(listen_socket, true);
	if(listen_event == NULL)
	{
		stop();
		return false;
	}
	
	//Start the threads
	running = true;
	for(int i=0; i<NUM_HTTP_WORKERS; ++i)
	{
		int res = pthread_create(&workers[i], NULL, worker_start, this);
		if(res != 0)
		{
			perror("pthread_create");
			stop();
			return false;
		}
		
		living_threads++;
		printf("HTTP Worker %d started\n", i);
	}
	
	return true;
}

//Stop the server
void HttpServer::stop()
{
	printf("Stopping http server...\n");
	clear_cache();
	
	//Stop running flag
	running = false;
	
	//Destroy epoll object
	if(epollfd != -1)
	{
		//FIXME: Purge old sockets here
		printf("Closing epoll\n");
		close(epollfd);
	}

	//Join with remaining worker threads
	printf("Killing workers\n");
	for(int i=0; i<living_threads; ++i)
	{
		printf("Stopping worker %d\n", i);
	
		if(pthread_join(workers[i], NULL) != 0)
		{
			perror("pthread_join");
		}
		
		if(pthread_detach(workers[i]) != 0)
		{
			perror("pthread_detach");
		}
		
		printf("HTTP Worker %d killed\n", i);
	}
	living_threads = 0;
		
	
	
	//Clean up any stray http events
	printf("Cleaning up stray connections\n");
	cleanup_events();
}

void HttpServer::accept_event(SocketEvent* event)
{

	int addr_size = sizeof(event->addr);
	int conn_fd = accept(event->socket_fd, (sockaddr*)&(event->addr), (socklen_t*)&addr_size);
	
	if(conn_fd == -1)
	{
		perror("accept");
	}
	else
	{
		printf("Got connection from address: ");

		sa_family_t fam = event->addr.ss_family;
		if(fam == AF_INET)
		{
			sockaddr_in *in_addr = (sockaddr_in*)(void*)&(event->addr);
			
			uint8_t* ip = (uint8_t*)(void*)&in_addr->sin_addr.s_addr;
			uint16_t port = in_addr->sin_port;
			
			for(int i=0; i<4; ++i)
			{
				if(i != 0)
					printf(".");
				printf("%d", ip[i]);
			}
			printf(":%d\n", port);
		}
		else if(fam == AF_INET6)
		{
			sockaddr_in6 *in_addr = (sockaddr_in6*)(void*)&(event->addr);
			
			uint8_t* ip = in_addr->sin6_addr.s6_addr;
			uint16_t port = in_addr->sin6_port;
			
			for(int i=0; i<8; ++i)
			{
				if(i != 0)
					printf(".");
				printf("%d", ip[i]);
			}
			printf(":%d\n", port);
		}

		//FIXME: Maybe check black list here, deny connections from ahole ip addresses
	
		if(create_event(conn_fd) == NULL)
		{
			printf("Error accepting connection\n");
		}
	}
}

void HttpServer::process_cached_request(SocketEvent* event, char* req, int req_len)
{
	printf("Got request: %s\n", req);

	//Set the socket event properties
	event->state = Sending;
	event->free_send_buffer = false;
	event->send_buf_start = const_cast<char*>(get_cached_response(req, req_len, &event->pending_bytes));
	event->send_buf_cur = event->send_buf_start;
	
	//Add back to IO queue
	epoll_event ev;
	ev.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
	ev.data.fd = event->socket_fd;
	ev.data.ptr = event;
	if(epoll_ctl(epollfd, EPOLL_CTL_MOD, event->socket_fd, &ev) == -1)
	{
		perror("epoll_ctl");
		dispose_event(event);
	}
}

//Process the pending request
void HttpServer::process_command(SocketEvent* event)
{
	printf("Got a command -- not implemented yet\n");
	dispose_event(event);
}

//Process a header request
void HttpServer::process_header(SocketEvent* event, bool do_recv)
{
	printf("Processing header\n");

	if(do_recv) do
	{
		int buf_len = event->recv_buf_size - (int)(event->recv_buf_cur - event->recv_buf_start);
		int len = recv(event->socket_fd, event->recv_buf_cur, buf_len, 0);
	
		if(len < 0)
		{
			if(errno == EAGAIN)
				continue;
			perror("recv");
			dispose_event(event);
			return;
		}
		else if(len == 0)
		{
			dispose_event(event);
			return;
		}
	
		event->recv_buf_cur += len;
		
	} while(errno != EAGAIN);
		
	//Try parsing http header
	char *req, *content_ptr;
	int req_len;
	int res = event->try_parse(&req, &req_len, &content_ptr, &event->content_length);
	
	if(res == 1)	//Case: successful parse
	{
		if(event->content_length > 0)
		{
			//Number of extra bytes read into the post section
			int extra_data = (int)(event->recv_buf_cur - content_ptr);

		
			if(event->content_length + (int)(event->recv_buf_cur - event->recv_buf_start) > event->recv_buf_size)
			{
				//FIXME: Maybe resize the buffer here if the request is not too big?
			
				dispose_event(event);
				return;
			}
			else
			{
				//Shift buffer backwards to overwrite http header
				memmove(event->recv_buf_start, content_ptr, extra_data);
				event->recv_buf_cur = event->recv_buf_start + extra_data;
			}

			//Need to do a send request
			event->pending_bytes = max(event->content_length - extra_data, 0);
			
			if(event->pending_bytes == 0)
			{
				process_command(event);
			}
			else
			{
				//Need to recieve more events			
				event->state = Receiving;
			
				//Add epoll event
				epoll_event ev;
				ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
				ev.data.fd = event->socket_fd;
				ev.data.ptr = event;
				if(epoll_ctl(epollfd, EPOLL_CTL_MOD, event->socket_fd, &ev) == -1)
				{
					perror("epoll_ctl");
					dispose_event(event);
				}
			}
		}
		else
		{
			process_cached_request(event, req, req_len);
		}
	}
	else if(res == -1)
	{
		//Header not yet complete, keep reading
		epoll_event ev;
		ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
		ev.data.fd = event->socket_fd;
		ev.data.ptr = event;
		if(epoll_ctl(epollfd, EPOLL_CTL_MOD, event->socket_fd, &ev) == -1)
		{
			perror("epoll_ctl");
			dispose_event(event);
		}
	}
	else 
	{
		printf("Bad header\n");
		dispose_event(event);
	}
}

void HttpServer::process_recv(SocketEvent* event)
{
	printf("Recieving bytes...\n");
	
	do
	{
		int len = recv(event->socket_fd, event->recv_buf_cur, event->pending_bytes, 0);
	
		if(len < 0)
		{
			if(errno == EAGAIN)
				continue;
			perror("recv");
			dispose_event(event);
			return;
		}
		else if(len == 0)
		{
			dispose_event(event);
			return;
		}
	
		event->pending_bytes -= len;
		event->recv_buf_cur += len;
		
	} while(errno != EAGAIN);
	
	if(event->pending_bytes == 0)
	{
		process_command(event);
	}
	else
	{
		//Not done reading, continue
		epoll_event ev;
		ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
		ev.data.fd = event->socket_fd;
		ev.data.ptr = event;
		if(epoll_ctl(epollfd, EPOLL_CTL_MOD, event->socket_fd, &ev) == -1)
		{
			perror("epoll_ctl");
			dispose_event(event);
		}
	}
}

//Process a send event
void HttpServer::process_send(SocketEvent* event)
{
	do
	{
		int len = send(event->socket_fd, event->send_buf_cur, event->pending_bytes, 0);
		
		if(len < 0)
		{
			if(errno == EAGAIN);
				continue;
			perror("send");
			dispose_event(event);
			return;
		}
		else if(len == 0)
		{
			dispose_event(event);
			return;
		}
		
		event->pending_bytes -= len;
		event->send_buf_cur += len;
	} while(errno == EAGAIN);


	if(event->pending_bytes > 0)
	{
		epoll_event ev;
		ev.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
		ev.data.fd = event->socket_fd;
		ev.data.ptr = event;
		
		if(epoll_ctl(epollfd, EPOLL_CTL_MOD, event->socket_fd, &ev) == -1)
		{
			dispose_event(event);
			return;
		}
	}
	else
	{
		//Send complete
	
		//Clean up send buffer
		if(event->free_send_buffer)
		{
			free(event->send_buf_start);
		}		
		event->free_send_buffer = false;
		event->send_buf_start = event->send_buf_cur = NULL;
	
		//Otherwise, start processing the next request
		process_header(event, false);
	}
}

//Worker boot strap
void* HttpServer::worker_start(void* arg)
{
	((HttpServer*)arg)->worker_loop();
	
	printf("Worker stopped\n");
	
	pthread_exit(0);
	return NULL;
}

//Run the worker loop
void HttpServer::worker_loop()
{
	epoll_event events[MAX_EPOLL_EVENTS];

	while(running)
	{
		int nfds = epoll_wait(epollfd, events, MAX_EPOLL_EVENTS, EPOLL_TIMEOUT);
		if(nfds == -1)
		{
			if(errno == EINTR)
			{
				continue;
			}
			break;
		}
	
		for(int i=0; i<nfds; ++i)
		{
			SocketEvent* event = (SocketEvent*)events[i].data.ptr;
			
			if( (events[i].events & EPOLLHUP) ||
				(events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLRDHUP) )
			{
				dispose_event(event);
			}
			else
			{
				switch(event->state)
				{
					case Listening:
						accept_event(event);
					break;
					
					case WaitForHeader:
						process_header(event);
					break;
					
					case Receiving:
						process_recv(event);
					break;
					
					case Sending:
						process_send(event);
					break;
				}
			}
		}
	}
}

