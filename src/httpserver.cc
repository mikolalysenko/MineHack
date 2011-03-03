#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <map>
#include <vector>
#include <string>

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/io/gzip_stream.h>

#include <tcutil.h>

#include "constants.h"
#include "config.h"
#include "httpserver.h"
#include "misc.h"

#include "network.pb.h"

using namespace std;
using namespace tbb;
using namespace Game;


//Uncomment this line to get dense logging for the web server
#define SERVER_DEBUG 1

#ifndef SERVER_DEBUG
#define DEBUG_PRINTF(...)
#else
#define DEBUG_PRINTF(...)  fprintf(stderr,__VA_ARGS__)
#endif


//-------------------------------------------------------------------
// Constructor/destructor pairs
//-------------------------------------------------------------------

//Initialize SocketEvent fields
Socket::Socket(int fd, bool listener) : socket_fd(fd)
{
	if(listener)
	{
		state = SocketState_Listening;
	}
	else
	{
		state = SocketState_WaitForHeader;
		inp.size = RECV_BUFFER_SIZE;
		inp.buf_start = (char*)malloc(RECV_BUFFER_SIZE);
		inp.buf_cur = inp.buf_start;
	}
}

//Destroy http event
Socket::~Socket()
{
	if(socket_fd != -1)
	{
		DEBUG_PRINTF("Closing socket: %d\n", socket_fd);
		close(socket_fd);
	}
}


//Http server constructor
HttpServer::HttpServer(Config* cfg, command_func cback) :
	config(cfg), 
	command_callback(cback)
{
}

//-------------------------------------------------------------------
// State management
//-------------------------------------------------------------------

//Start the server
bool HttpServer::start()
{
	DEBUG_PRINTF("Starting http server...\n");
	
	epollfd = -1;
	living_threads = 0;
	
	//Initialize cache
	recache();	
	
	//Create listen socket
	DEBUG_PRINTF("Creating listen socket\n");
	int listen_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
	if(listen_socket_fd == -1)
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
	if(setsockopt(listen_socket_fd, SOL_SOCKET,  SO_REUSEADDR, &optval, sizeof(optval)) == -1)
	{
		perror("setsockopt");
		stop();
		return false;
	}
	
	if(bind(listen_socket_fd, (sockaddr *) (void*)&addr, sizeof(addr)) == -1)
	{
		perror("bind");
		stop();
		return false;
	}

	//Listen
	if (listen(listen_socket_fd, LISTEN_BACKLOG) == -1) {
	    perror("listen()");
	    stop();
	    return false;
	}
	

	//Create epoll object
	DEBUG_PRINTF("Creating epoll object\n");
	epollfd = epoll_create(MAX_CONNECTIONS);
	if(epollfd == -1)
	{
		perror("epoll_create");
		stop();
		return false;
	}

	//Add listen socket event
	sockaddr_storage st_addr;
	*((sockaddr_in*)(void*)&st_addr) = addr;
	auto listen_socket = create_socket(listen_socket_fd, true, &st_addr);
	if(listen_socket == NULL)
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
		DEBUG_PRINTF("HTTP Worker %d started\n", i);
	}
	
	return true;
}

//Stop the server
void HttpServer::stop()
{
	DEBUG_PRINTF("Stopping http server...\n");
	
	//Stop running flag
	running = false;
	
	//Destroy epoll object
	if(epollfd != -1)
	{
		//FIXME: Purge old sockets here
		DEBUG_PRINTF("Closing epoll\n");
		close(epollfd);
	}

	//Join with remaining worker threads
	DEBUG_PRINTF("Killing workers\n");
	for(int i=0; i<living_threads; ++i)
	{
		DEBUG_PRINTF("Stopping worker %d\n", i);
	
		if(pthread_join(workers[i], NULL) != 0)
		{
			perror("pthread_join");
		}
		
		DEBUG_PRINTF("HTTP Worker %d killed\n", i);
	}
	living_threads = 0;
	
	//Clean up any stray http events
	DEBUG_PRINTF("Cleaning up stray connections\n");
	cleanup_sockets();
		
	//Clean up any old caches
	DEBUG_PRINTF("Cleaning up extra cache stuff\n");
	cleanup_cache();
}


//-------------------------------------------------------------------
// Static cache management
//-------------------------------------------------------------------

void HttpServer::recache()
{
	//Just update the cache in place
	//Bug: This will not clear out old files, so if some data gets deleted or renamed on the server, it will still be visible after a recache.
	cache_directory(cached_responses, config->readString("wwwroot"));
}

void HttpServer::cleanup_cache()
{
	//Read out all the keys
	vector<string> old_keys;	
	for(auto iter = cached_responses.begin(); iter!=cached_responses.end(); ++iter)
	{
		old_keys.push_back(iter->first);
	}
	
	//Erase the keys from the map one by one
	for(auto iter = old_keys.begin(); iter!=old_keys.end(); ++iter)
	{
		concurrent_hash_map<string, HttpResponse>::accessor acc;
		if(!cached_responses.find(acc, *iter))
			continue;
		free(acc->second.buf);
		cached_responses.erase(acc);
	}
}

//Retrieves a response
bool HttpServer::get_cached_response(string const& request, concurrent_hash_map<string, HttpResponse>::const_accessor& acc)
{
	if(!cached_responses.find(acc, request))
	{
		return cached_responses.find(acc, "404.html");
	}
	return true;
}

//-------------------------------------------------------------------
// Socket management
//-------------------------------------------------------------------

//Creates an http event
Socket* HttpServer::create_socket(int fd, bool listener, sockaddr_storage *addr)
{
	Socket* result = new Socket(fd, listener);
	result->socket_fd = fd;
	
	
	if(addr != NULL)
	{
		result->addr = *addr;
	}
	
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
	
	epoll_event ev;
	ev.events = listener ? EPOLLIN : (EPOLLIN | EPOLLET | EPOLLONESHOT);
	ev.data.fd = fd;
	ev.data.ptr = result;

	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1)
	{
		perror("epoll_add");
		delete result;
		return NULL;
	}

	//Store pointer in map
	concurrent_hash_map<int, Socket*>::accessor acc;
	if(!socket_set.insert(acc, make_pair(fd, result)))
	{
		DEBUG_PRINTF("File descriptor is already in use!\n");
		dispose_socket(result);
		return NULL;
	}
	
	return result;
}

//Notifies a socket of a change
bool HttpServer::notify_socket(Socket* socket)
{
	epoll_event ev;
	
	switch(socket->state)
	{
		case SocketState_Listening:
			ev.events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
		break;
		
		case SocketState_PostRecv:
		case SocketState_WaitForHeader:
			ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
		break;
		
		case SocketState_WebSocketHandshake:
		case SocketState_PostReply:
		case SocketState_CachedReply:
			ev.events = EPOLLOUT | EPOLLET | EPOLLONESHOT | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
		break;
	}
	
	ev.data.fd = socket->socket_fd;
	ev.data.ptr = socket;
	
	if(epoll_ctl(epollfd, EPOLL_CTL_MOD, socket->socket_fd, &ev) == -1)
	{
		perror("epoll_ctl");
		return false;
	}
	
	return true;
}

//Disposes of an http event
void HttpServer::dispose_socket(Socket* socket)
{
	DEBUG_PRINTF("Disposing socket\n");
	epoll_ctl(epollfd, EPOLL_CTL_DEL, socket->socket_fd, NULL);	
	socket_set.erase(socket->socket_fd);
	delete socket;
}


//Clean up all events in the server
// (This is always called after epoll and all workers have stopped)
void HttpServer::cleanup_sockets()
{
	for(auto iter = socket_set.begin(); iter != socket_set.end(); ++iter)
	{
		delete (*iter).second;
	}
	socket_set.clear();
}


//-------------------------------------------------------------------
// Event handlers
//-------------------------------------------------------------------


void HttpServer::process_accept(Socket* socket)
{
	DEBUG_PRINTF("Got connection\n");

	//Accept connection
	sockaddr_storage addr;
	int addr_size = sizeof(addr), conn_fd = -1;
	
	conn_fd = accept(socket->socket_fd, (sockaddr*)&(addr), (socklen_t*)&addr_size);
	if(conn_fd == -1)
	{
		DEBUG_PRINTF("Accept error, errno = %d\n", errno);
		return;
	}
		
	//FIXME: Maybe check black list here, deny connections from ahole ip addresses
	
	auto res = create_socket(conn_fd, false, &addr);
	if(res == NULL)
	{
		DEBUG_PRINTF("Error accepting connection\n");
	}
	
	DEBUG_PRINTF("Connection successfully accepted, fd = %d\n", conn_fd);
}


//Process a header request
void HttpServer::process_header(Socket* socket)
{
	DEBUG_PRINTF("Processing header, fd = %d\n", socket->socket_fd);

	char* header_end = max(socket->inp.buf_cur - 4, socket->inp.buf_start);
	
	int buf_len = socket->inp.size - (int)(socket->inp.buf_cur - socket->inp.buf_start);
	if(buf_len <= 0)
	{
		DEBUG_PRINTF("Client packet overflowed, discarding\n");
		dispose_socket(socket);
		return;
	}
	
	int len = recv(socket->socket_fd, socket->inp.buf_cur, buf_len, 0);
	if(len < 0 )
	{
		if(errno == EAGAIN)
		{
			DEBUG_PRINTF("Socket not yet ready, readding\n");
			notify_socket(socket);
			return;
		}
		perror("recv");
		dispose_socket(socket);
		return;
	}
	else if(len == 0)
	{
		DEBUG_PRINTF("Remote end hung up during header transmission\n");
		dispose_socket(socket);
		return;
	}

	socket->inp.buf_cur += len;
	
	//Scan for end of header
	int num_eol = 0;	
	for(; header_end<socket->inp.buf_cur; ++header_end)
	{
		switch(*header_end)
		{
			case '\r': break;
			case '\n':
				num_eol++;
			break;
			default:
				num_eol = 0;
			break;
		}
		
		if(num_eol >= 2)
		{
			break;
		}
	}
	header_end++;
	
	//Incomplete http header
	if(num_eol < 2)
	{
		DEBUG_PRINTF("Header incomplete, continuing wait for header\n");
		if(!notify_socket(socket))
		{
			dispose_socket(socket);
		}
		return;
	}
	
	//Parse header
	int header_size = (int)(header_end - socket->inp.buf_start);
	parse_http_request(socket->inp.buf_start, header_end, socket->request);
	
	switch(socket->request.type)
	{
	case HttpRequestType_Bad:
	{
		DEBUG_PRINTF("Bad HTTP header\n");
		dispose_socket(socket);
	}
	break;
	
	case HttpRequestType_Get:
	{
		DEBUG_PRINTF("Got cached HTTP request: %s\n", socket->request.url.c_str());
		
		if(!get_cached_response(socket->request.url, socket->cached_response))
		{
			dispose_socket(socket);
			return;
		}
		
		socket->state = SocketState_CachedReply;
		socket->outp.pending = socket->cached_response->second.size;
		if(!notify_socket(socket))
		{
			dispose_socket(socket);
			return;
		}
	}
	break;
		
	case HttpRequestType_Post:
	{
		DEBUG_PRINTF("Got post HTTP request\n");
		
		int total_size = socket->request.content_length + header_size;
		
		if(socket->request.content_length <= 0 || 
			socket->request.content_length + header_size > socket->inp.size)
		{
			//FIXME: Send error reply
			dispose_socket(socket);
			return;
		}

		int pending_bytes = total_size - (int)(socket->inp.buf_cur - socket->inp.buf_start);
		
		if(pending_bytes <= 0)
		{
			//Handle post request immediately
			process_post(socket);
			return;
		}
		else
		{
			socket->state = SocketState_PostRecv;
			socket->inp.pending = pending_bytes;
			
			if(!notify_socket(socket))
			{
				dispose_socket(socket);
			}
			return;
		}
	}
	break;
	
	case HttpRequestType_WebSocket:
	{
		DEBUG_PRINTF("Got a web socket connection\n");
		
		//Make sure we read the extra 8 bytes for the connection key
		if((int)(socket->inp.buf_cur - socket->request.content_ptr) < 8)
		{
			if(!notify_socket(socket))
				dispose_socket(socket);
			return;
		}
		
		//Create the handshake packet
		auto ws_handshake = http_websocket_handshake(socket->request);
		if(ws_handshake.buf == NULL)
		{
			DEBUG_PRINTF("Bad WebSocket handshake\n");
			dispose_socket(socket);
			return;
		}
		
		//FIXME: Register web socket with application here
		
		socket->state = SocketState_WebSocketHandshake;
		socket->outp.size = ws_handshake.size;
		socket->outp.pending = ws_handshake.size;		
		socket->outp.buf_start = ws_handshake.buf;
		socket->outp.buf_cur = ws_handshake.buf;
		
		if(!notify_socket(socket))
		{
			dispose_socket(socket);
		}
	}
	break;
	}
}

void HttpServer::process_reply_cached(Socket* socket)
{
	DEBUG_PRINTF("Sending cached reply, pending_bytes = %d\n", socket->outp.pending);
	
	auto response = socket->cached_response->second;
	
	int offset = response.size - socket->outp.pending;
	int len = send(socket->socket_fd, response.buf + offset, socket->outp.pending, 0);
	
	if(len < 0)
	{
		if(errno == EAGAIN);
		{
			DEBUG_PRINTF("Send incomplete, retrying\n");
			notify_socket(socket);
			return;
		}
		perror("send");
		socket->cached_response.release();
		dispose_socket(socket);
		return;
	}
	else if(len == 0)
	{
		DEBUG_PRINTF("Remote end hung up\n");
		socket->cached_response.release();
		dispose_socket(socket);
		return;
	}
	
	socket->outp.pending -= len;

	if(socket->outp.pending <= 0)
	{
		DEBUG_PRINTF("Send complete\n");
		socket->cached_response.release();
		dispose_socket(socket);
		return;
	}
	
	DEBUG_PRINTF("More to send, continuing\n");	
	if(!notify_socket(socket))
	{
		DEBUG_PRINTF("Notify failed\n");
		socket->cached_response.release();
		dispose_socket(socket);
		return;
	}
}

void HttpServer::process_recv(Socket* socket)
{
	DEBUG_PRINTF("Receiving, pending_bytes = %d\n", socket->inp.pending);
	
	int len = recv(socket->socket_fd, socket->inp.buf_cur, socket->inp.pending, 0);
	
	if(len < 0)
	{
		if(errno == EAGAIN);
		{
			DEBUG_PRINTF("Recv incomplete, retrying\n");
			notify_socket(socket);
			return;
		}
		perror("recv");
		dispose_socket(socket);
		return;
	}
	else if(len == 0)
	{
		DEBUG_PRINTF("Remote end hung up\n");
		dispose_socket(socket);
		return;
	}
	
	socket->inp.pending -= len;
	socket->inp.buf_cur += len;

	if(socket->inp.pending <= 0)
	{
		DEBUG_PRINTF("Recv complete\n");
		
		switch(socket->state)
		{	
		case SocketState_PostRecv:
			process_post(socket);
			return;
		break;
		}

		DEBUG_PRINTF("Unknown state, destroy socket\n");
		dispose_socket(socket);		
		return;
	}
	
	DEBUG_PRINTF("More to recv, continuing\n");
	if(!notify_socket(socket))
	{
		DEBUG_PRINTF("Notify failed\n");
		dispose_socket(socket);
		return;
	}
}

void HttpServer::process_send(Socket* socket)
{
	DEBUG_PRINTF("Sending, pending_bytes = %d\n", socket->outp.pending);
	
	int len = send(socket->socket_fd, socket->outp.buf_cur, socket->outp.pending, 0);
	
	if(len < 0)
	{
		if(errno == EAGAIN);
		{
			DEBUG_PRINTF("Send incomplete, retrying\n");
			notify_socket(socket);
			return;
		}
		perror("send");
		dispose_socket(socket);
		return;
	}
	else if(len == 0)
	{
		DEBUG_PRINTF("Remote end hung up\n");
		dispose_socket(socket);
		return;
	}
	
	socket->outp.pending -= len;
	socket->outp.buf_cur += len;

	if(socket->outp.pending <= 0)
	{
		DEBUG_PRINTF("Send complete\n");
		
		
		switch(socket->state)
		{	
		case SocketState_PostReply:
			dispose_socket(socket);
			return;
		break;
		
		case SocketState_WebSocketHandshake:
		{
			DEBUG_PRINTF("WebSocket handshake complete\n");
			
			//FIXME: Implement frame protocol here
			
			dispose_socket(socket);
			return;
		}
		break;
		}
		
		dispose_socket(socket);
		return;
	}
	
	DEBUG_PRINTF("More to send, continuing\n");
	if(!notify_socket(socket))
	{
		DEBUG_PRINTF("Notify failed\n");
		dispose_socket(socket);
		return;
	}
}

//Process a post request
void HttpServer::process_post(Socket* socket)
{
	DEBUG_PRINTF("Processinng a post request, len = %d\n", socket->request.content_length);
	
	//Parse client packet
	auto P = ScopeDelete<Network::ClientPacket>(new Network::ClientPacket());
	if(!P.ptr->ParseFromArray(socket->request.content_ptr, socket->request.content_length))
	{
		DEBUG_PRINTF("Failed to parse post request\n");
		dispose_socket(socket);
		return;
	}
	
	
	auto reply = ScopeDelete<Network::ServerPacket>((*command_callback)(P.ptr));
	if(reply.ptr == NULL)
	{
		dispose_socket(socket);
		return;
	}
	
	auto resp = http_serialize_protobuf(reply.ptr);
	
	if(resp.buf == NULL)
	{
		dispose_socket(socket);
		return;
	}
	
	socket->state = SocketState_PostReply;
	socket->outp.size = resp.size;
	socket->outp.pending = resp.size;
	socket->outp.buf_start = resp.buf;
	socket->outp.buf_cur = resp.buf;
	
	if(!notify_socket(socket))
	{
		dispose_socket(socket);
	}
}

//-------------------------------------------------------------------
// Worker loop
//-------------------------------------------------------------------


//Worker boot strap
void* HttpServer::worker_start(void* arg)
{
	DEBUG_PRINTF("Worker started\n");
	((HttpServer*)arg)->worker_loop();
	DEBUG_PRINTF("Worker stopped\n");
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
			perror("epoll");
			
			DEBUG_PRINTF("EPOLL WORKER DIED!  OH SHIT THE THING IS GONNA CRASH!!!!");
			break;
		}
	
		for(int i=0; i<nfds; ++i)
		{
			Socket* socket = (Socket*)events[i].data.ptr;
			
			if( (events[i].events & EPOLLHUP) ||
				(events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLRDHUP) )
			{
				DEBUG_PRINTF("Remote end hung up\n");
				dispose_socket(socket);
			}
			else switch(socket->state)
			{
				case SocketState_Listening:
					process_accept(socket);
				break;
				
				case SocketState_WaitForHeader:
					process_header(socket);
				break;
				
				case SocketState_CachedReply:
					process_reply_cached(socket);
				break;
				
				case SocketState_PostRecv:
					process_recv(socket);
				break;
				
				case SocketState_PostReply:
					process_send(socket);
				break;
			}
		}
	}
}

