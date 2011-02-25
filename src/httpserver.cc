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
	"Content-Encoding: gzip\n"
	"Content-Length: %d\n\n";



//Initialize HttpEvent fields
HttpEvent::HttpEvent() : 
	socket_fd(-1), 
	state(WaitForHeader),
	no_delete(false),
	buf_size(RECV_BUFFER_SIZE),
	buf_start((char*)malloc(RECV_BUFFER_SIZE))
{
	buf_cur = buf_start;
}

//Destroy http event
HttpEvent::~HttpEvent()
{
	if(socket_fd != -1)
	{
		printf("Closing socket: %d\n", socket_fd);
		close(socket_fd);
	}
	
	if(buf_start != NULL && !no_delete)
	{
		free(buf_start);
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
int HttpEvent::try_parse(char** request, int* request_len, char** content_start, int* content_length)
{
	//FIXME: Put a cap on the end_ptr length to limit ddos attacks
	char *ptr     = (char*)buf_start,
		 *end_ptr = (char*)buf_cur;
		  
	int len = (int)(end_ptr - ptr);
	if(len < 7)
		return -1;
	
	bool post = false;
	if(ptr[0] == 'G')
	{
		ptr += 5;
	}
	else if(ptr[0] == 'P')
	{
		ptr += 6;
		post = true;
	}
	else
	{
		printf("UNKOWN MESSAGE TYPE\n");
		return 0;
	}
	
	//Set request
	*request = ptr;
	
	//Read to end of request
	char c;
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
	
	//For get stuff, we are done
	if(!post)
	{
		*content_length = 0;
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
HttpEvent* HttpServer::create_event(int fd, bool add_epoll)
{
	HttpEvent* result = new HttpEvent();
	result->socket_fd = fd;
	
	//Store pointer in map
	pthread_spin_lock(&event_map_lock);
	bool success = tcmapputkeep(event_map, &fd, sizeof(fd), &result, sizeof(result));
	pthread_spin_unlock(&event_map_lock);
	
	if(success)
	{
		if(add_epoll)
		{
			//Set socket to non blocking
			int flags = fcntl(fd, F_GETFL, 0);
 			if (flags == -1)
        		flags = 0;
        	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        	
			epoll_event ev;
			ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
			ev.data.fd = fd;
			ev.data.ptr = result;
			epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
		}
		
		return result;
	}
	
	delete result;
	return NULL;
}

//Disposes of an http event
void HttpServer::dispose_event(HttpEvent* event)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, event->socket_fd, NULL);
	pthread_spin_lock(&event_map_lock);
	tcmapout(event_map, &event->socket_fd, sizeof(int));
	pthread_spin_unlock(&event_map_lock);
	delete event;
}

//Clean up all events in the server
void HttpServer::cleanup_events()
{
	TCLIST* events = tcmapvals(event_map);
	while(true)
	{
		int sz;
		void* ptr = tclistpop(events, &sz);
		if(ptr == NULL)
			break;
			
		HttpEvent* event = *((HttpEvent**)ptr);
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
	auto listen_event = create_event(listen_socket, false);
	listen_event->state = Listening;
	
	epoll_event event;
	event.events = EPOLLIN | EPOLLPRI;
	event.data.ptr = listen_event;

	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_socket, &event) == -1)
	{
		perror("epoll_add");
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
		pthread_join(workers[i], NULL);
		pthread_detach(workers[i]);	
		printf("HTTP Worker %d killed\n", i);
	}
	living_threads = 0;
		
	
	
	//Clean up any stray http events
	printf("Cleaning up stray connections\n");
	cleanup_events();
}

void HttpServer::accept_event(HttpEvent* event)
{
	printf("GOT CONNECTION\n");

	sockaddr_storage addr;
	int addr_size = sizeof(addr);
	int conn_fd = accept(event->socket_fd, (sockaddr*)&addr, (socklen_t*)&addr_size);
	
	//FIXME: Check if address is black listed
	
	if(conn_fd == -1)
	{
		perror("accept");
	}
	else
	{
		create_event(conn_fd);
	}
}

void HttpServer::process_cached_request(HttpEvent* event, char* req, int req_len)
{
	printf("Got request: %s\n", req);

	//Pull the cached response out
	free(event->buf_start);
	event->no_delete = true;
	event->buf_start = const_cast<char*>(get_cached_response(req, req_len, &event->pending_bytes));
	
	//Update state
	event->state = Sending;
	event->buf_cur = event->buf_start;
	
	//Add back to IO queue
	epoll_event ev;
	ev.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
	ev.data.fd = event->socket_fd;
	ev.data.ptr = event;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, event->socket_fd, &ev);
}

void HttpServer::process_header(HttpEvent* event)
{
	printf("Processing header\n");

	do
	{
		int buf_len = event->buf_size - (int)(event->buf_cur - event->buf_start);
		int len = recv(event->socket_fd, event->buf_cur, buf_len, 0);
	
		if(len < 0)
		{
			if(errno == EAGAIN)
				continue;
			perror("recv");
			dispose_event(event);
			return;
		}
	
		event->buf_cur += len;
		
	} while(errno != EAGAIN);
		
	//Try parsing http header
	char *req;
	int req_len;
	int res = event->try_parse(&req, &req_len, &event->content_ptr, &event->content_length);
	
	if(res == 1)	//Case: successful parse
	{
		if(event->content_length > 0)
		{
			if(event->content_length + (int)(event->buf_cur - event->buf_start) > RECV_BUFFER_SIZE)
			{
				dispose_event(event);
				return;
			}
		
			//Resize the buffer to clip out the header
			int extra_data = (int)(event->buf_cur - event->content_ptr);
			event->pending_bytes = event->content_length - extra_data;
			
			//Update state
			event->state = Receiving;
			
			//Add epoll event
			epoll_event ev;
			ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
			ev.data.fd = event->socket_fd;
			ev.data.ptr = event;
			epoll_ctl(epollfd, EPOLL_CTL_MOD, event->socket_fd, &ev);
		}
		else
		{
			//Process the event
			event->state = Processing;
			process_cached_request(event, req, req_len);
		}
	}
	else if(res == -1)
	{
		epoll_event ev;
		ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
		ev.data.fd = event->socket_fd;
		ev.data.ptr = event;
		epoll_ctl(epollfd, EPOLL_CTL_MOD, event->socket_fd, &ev);
	}
	else 
	{
		dispose_event(event);
	}
}

void HttpServer::process_recv(HttpEvent* event)
{
	//Not implemented yet
	dispose_event(event);
}

//Process a send event
void HttpServer::process_send(HttpEvent* event)
{
	int len = send(event->socket_fd, event->buf_cur, event->pending_bytes, 0);
	event->buf_cur += len;
	event->pending_bytes -= len;

	if(event->pending_bytes <= 0 || len <= 0)
	{
		dispose_event(event);
	}
	else
	{
		epoll_event ev;
		ev.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
		ev.data.fd = event->socket_fd;
		ev.data.ptr = event;
		epoll_ctl(epollfd, EPOLL_CTL_MOD, event->socket_fd, &ev);
	}
}

//Worker boot strap
void* HttpServer::worker_start(void* arg)
{
	((HttpServer*)arg)->worker_loop();
	
	printf("WORKER DEAD\n");
	
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
			HttpEvent* event = (HttpEvent*)events[i].data.ptr;
			
			if( (events[i].events & EPOLLHUP) ||
				(events[i].events & EPOLLERR) )
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

