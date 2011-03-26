#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

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



//Uncomment this line to get dense logging for the web server
//#define SERVER_DEBUG 1

#ifndef SERVER_DEBUG
#define DEBUG_PRINTF(...)
#else
#define DEBUG_PRINTF(...)  fprintf(stderr,__VA_ARGS__)
#endif


namespace Game {

//-------------------------------------------------------------------
// Constructor/destructor pairs
//-------------------------------------------------------------------

//Initialize SocketEvent fields
Socket::Socket(int fd) : socket_fd(fd)
{
}

//Destroy http event
Socket::~Socket()
{
	if(socket_fd != -1)
	{
		close(socket_fd);
	}
}


//Http server constructor
HttpServer::HttpServer(Config* cfg, command_func cback, websocket_func wcback) :
	config(cfg), 
	command_callback(cback),
	websocket_callback(wcback)
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
	int listen_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
	if(listen_socket_fd == -1)
	{
		perror("socket");
		stop();
		return false;
	}

	//Bind socket address
	auto portnum = config->readInt(string("listenport"));
	sockaddr_in addr;
	addr.sin_addr.s_addr	= INADDR_ANY;
	addr.sin_port 			= htons(portnum);
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
	Socket* result = new Socket(fd);
	if(addr != NULL)
	{
		result->addr = *addr;
	}

	if(!listener)
	{
		result->state = SocketState_WaitForHeader;
		result->inp.init_buffer(RECV_BUFFER_SIZE);
	}
	else
	{
		result->state = SocketState_Listening;
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
	
	linger L;
	L.l_onoff = true;
	L.l_linger = 10;
	setsockopt(fd, SOL_SOCKET, SO_LINGER, &L, sizeof(L));
	
	//Store pointer in map
	if(!socket_set.insert(make_pair(fd, result)))
	{
		DEBUG_PRINTF("File descriptor is already in use!\n");
		delete result;
		return NULL;
	}
	
	//Add to epoll
	epoll_event ev;
	if(listener)
	{
		ev.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP; 
	}
	else
	{
		ev.events =  EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
	}
	ev.data.fd = fd;
	ev.data.ptr = result;

	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1)
	{
		DEBUG_PRINTF("EPOLL FAILED!?!?!,  This should not happen.\n");
		perror("epoll_add");
		socket_set.erase(fd);
		delete result;
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
			ev.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
		break;
		
		case SocketState_WebSocketRecv:
		case SocketState_PostRecv:
		case SocketState_WaitForHeader:
			ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
		break;
		
		case SocketState_WebSocketSend:
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
	DEBUG_PRINTF("Disposing socket, fd = %d\n", socket->socket_fd);
	socket_set.erase(socket->socket_fd);
	epoll_ctl(epollfd, EPOLL_CTL_DEL, socket->socket_fd, NULL);	
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
// Web socket event handlers
// This situation is kind of a bastardized version of producer-consumer synchronization
//	The problem is that both the consumers and producers can spontaneously die, and so we have to
//	deal with destruction of these objects in some nice way without garbage collection.
//	Blah.
//-------------------------------------------------------------------


void HttpServer::initialize_websocket(Socket* socket)
{
	int send_fd = dup(socket->socket_fd);
	if(send_fd == -1)
	{
		perror("dup");
		dispose_socket(socket);
	}

	auto sender = new Socket(send_fd);
	
	DEBUG_PRINTF("Creating websocket, receiver fd = %d, sender fd = %d\n", socket->socket_fd, sender->socket_fd);
	
	//Initialize state
	socket->state = SocketState_WebSocketRecv;
	sender->state = SocketState_WebSocketSend;
	
	//Update buffers
	sender->outp.size = socket->outp.size;
	sender->outp.pending = 0;
	sender->outp.buf_cur = sender->outp.buf_start = socket->outp.buf_start;
	
	//Clear out socket's send buffer
	socket->outp.size = 0;
	socket->outp.pending = 0;
	socket->outp.buf_cur = socket->outp.buf_start = NULL;
	
	//Reset input pointer
	socket->inp.pending = 0;
	socket->inp.buf_cur = socket->inp.buf_start;

	//Store pointer in map
	if(!socket_set.insert(make_pair(sender->socket_fd, sender)))
	{
		DEBUG_PRINTF("File descriptor is already in use!\n");
		dispose_socket(socket);
		delete sender;
		return;
	}
	
	//Create web socket queue
	auto recv_q_impl = new WebSocketQueue_Impl();
	auto send_q_impl = new WebSocketQueue_Impl();
	
	//Bind queues
	socket->message_queue.attach(recv_q_impl);
	sender->message_queue.attach(send_q_impl);
	
	//Create web socket
	auto websocket = new WebSocket(
		this,
		send_q_impl,
		recv_q_impl,
		sender->socket_fd,
		sender);
	
	//Push a bum event into the sender queue for initialization purposes
	sender->message_queue.push_and_check_empty(NULL);

	//Add the sender to epoll
	epoll_event ev;
	ev.events = EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLET | EPOLLONESHOT;
	ev.data.fd = sender->socket_fd;
	ev.data.ptr = sender;
	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, sender->socket_fd, &ev) == -1)
	{
		perror("epoll_add");
		dispose_socket(socket);
		dispose_socket(sender);
		delete websocket;
		return;
	}

	//Try to register the websocket with the client
	if(!(websocket_callback)(socket->request, websocket))
	{
		DEBUG_PRINTF("Application rejected socket\n");
		dispose_socket(socket);
		delete websocket;
		return;
	}
	
	//Wake up receiver
	if(!notify_socket(socket))
	{
		DEBUG_PRINTF("Failed to notify receiver socket\n");
		dispose_socket(socket);
		
		//Failed, but can't dispose either sender or websocket, so just have to wait for them to timeout
		return;
	}
}

void notify_websocket_send(HttpServer* server, int send_fd, Socket* socket)
{
	//Verify that the socket is in the hash map
	concurrent_hash_map<int, Socket*>::const_accessor acc;
	if(!server->socket_set.find(acc, send_fd) ||
		acc->second != socket)
	{
		return;
	}
	if(!server->notify_socket(socket))
	{
		server->dispose_socket(socket);
	}
}

void HttpServer::handle_websocket_recv(Socket* socket)
{
	//While there are still frames to process:
	while(true)
	{
		DEBUG_PRINTF("Processing web socket recv, fd = %d\n", socket->socket_fd);

		if(socket->message_queue.ref_count() < 2)
		{
			DEBUG_PRINTF("Web socket died when receiving, fd = %d\n", socket->socket_fd);
			dispose_socket(socket);
			return;
		}
	
		//Try reading some bytes from the web socket
		int len = recv(socket->socket_fd, 
			socket->inp.buf_cur, 
			socket->inp.size - (int)(socket->inp.buf_cur - socket->inp.buf_start), 
			0);
	
		if(len < 0)
		{
			if(errno == EAGAIN);
			{
				if(!notify_socket(socket))
				{
					dispose_socket(socket);
				}
				return;
			}
			perror("recv");
			dispose_socket(socket);
			return;
		}
		else if(len == 0)
		{
			DEBUG_PRINTF("Remote end hung up during web socket recv, fd = %d\n", socket->socket_fd);
			dispose_socket(socket);
			return;
		}

		char* end_of_frame = socket->inp.buf_cur;	
		socket->inp.buf_cur += len;
		
		//Check frame start
		while(true)
		{
			uint8_t frame_type = (uint8_t)socket->inp.buf_start[0];
		
			if(frame_type == 0xff)
			{
				DEBUG_PRINTF("Connection closed\n");
				dispose_socket(socket);
				return;
			}
			
			//Scan for end of frame byte
			bool has_frame = false;
			while(end_of_frame < socket->inp.buf_cur)
			{
				if((uint8_t)(*(end_of_frame++)) == 0xff)
				{
					DEBUG_PRINTF("Found end of frame pointer\n");
					has_frame = true;
					break;
				}
			}
			if(!has_frame)
			{
				DEBUG_PRINTF("Frame incomplete, continuing\n");
				break;
			}
				
			//Process the input frame				
			int frame_size = (int)(end_of_frame - socket->inp.buf_start);
			auto packet = parse_ws_frame(socket->inp.buf_start+1, frame_size - 2);
			
			if(packet != NULL)
			{
				socket->message_queue.push_and_check_empty(packet);
			}
			else
			{
				DEBUG_PRINTF("Bad packet in websocket recv, fd = %d\n", socket->socket_fd);
			}
	
			//Shift queue backwards
			int extra_bytes = socket->inp.buf_cur - end_of_frame;
			memmove(socket->inp.buf_start, end_of_frame, extra_bytes);
			socket->inp.buf_cur -= frame_size;
			end_of_frame = socket->inp.buf_start + 1;
		}
	}
}

void HttpServer::handle_websocket_send(Socket* socket)
{

	while(true)
	{
		DEBUG_PRINTF("Processing web socket send, fd = %d\n", socket->socket_fd);
	
		if(socket->message_queue.ref_count() < 2)
		{
			DEBUG_PRINTF("Web socket died during send, fd = %d\n", socket->socket_fd);
			dispose_socket(socket);
			return;
		}

		//Check pending bytes
		if(socket->outp.pending == 0)
		{
			//Check if there are any messages left to send
			google::protobuf::Message* msg = NULL;
			if(!socket->message_queue.peek_if_full(msg))
				return;
			
			//Ignore null messages
			if(msg == NULL)
			{
				if(socket->message_queue.pop_and_check_empty(msg))
					return;
				continue;
			}
			
			//Serialize packet to output stream
			auto packet = (Network::ServerPacket*)msg;
			socket->outp.pending = serialize_ws_frame(packet, socket->outp.buf_start, socket->outp.size);
			socket->outp.buf_cur = socket->outp.buf_start;
			
			if(socket->outp.pending == 0)
				continue;
		}
	

		//Write pending bytes
		int len = send(socket->socket_fd, socket->outp.buf_cur, socket->outp.pending, MSG_NOSIGNAL);
	
		if(len < 0)
		{
			if(errno == EAGAIN);
			{
				notify_socket(socket);
				return;
			}
			perror("send");
			dispose_socket(socket);
			return;
		}
		else if(len == 0)
		{
			DEBUG_PRINTF("Remote end hung up during websocket send, fd = %d\n", socket->socket_fd);
			dispose_socket(socket);
			return;
		}
	
		socket->outp.pending -= len;
		socket->outp.buf_cur += len;
	
		//Check for send completion
		if(socket->outp.pending > 0)
		{
			DEBUG_PRINTF("Send incomplete\n");
			if(!notify_socket(socket))
				dispose_socket(socket);
			return;
		}
	
		//Send completed, release the message
		google::protobuf::Message* msg;
		bool done = socket->message_queue.pop_and_check_empty(msg);
		delete msg;
		if(done)
			return;
	}
}

WebSocket::WebSocket(HttpServer* server_, 
	WebSocketQueue_Impl* send_q_impl, 
	WebSocketQueue_Impl* recv_q_impl, 
	int send_fd_, 
	Socket* send_socket_) :
	server(server_),
	send_q(send_q_impl),
	recv_q(recv_q_impl),
	send_fd(send_fd_),
	send_socket(send_socket_)
{
}

WebSocket::~WebSocket()
{
	if(send_q.detach_and_check_empty())
		notify_websocket_send(server, send_fd, send_socket);
}


bool WebSocket::alive()
{
	return (send_q.ref_count() == 2) && (recv_q.ref_count() == 2);
}

bool WebSocket::send_packet(Network::ServerPacket* packet)
{
	bool empty = send_q.push_and_check_empty(packet);
	if(empty)
		notify_websocket_send(server, send_fd, send_socket);
	return true;
}

bool WebSocket::recv_packet(Network::ClientPacket*& packet)
{
	google::protobuf::Message* msg = NULL;
	bool result = recv_q.pop_if_full(msg);
	packet = (Network::ClientPacket*)msg;
	return result;
}



//-------------------------------------------------------------------
// Http event handlers
//-------------------------------------------------------------------


void HttpServer::process_accept(Socket* socket)
{
	DEBUG_PRINTF("Got connection\n");

	while(true)
	{
		//Accept connection
		sockaddr_storage addr;
		int addr_size = sizeof(addr), conn_fd = -1;
	
		conn_fd = accept(socket->socket_fd, (sockaddr*)&(addr), (socklen_t*)&addr_size);
		if(conn_fd == -1)
		{
			return;
		}
		
		//FIXME: Maybe check black list here, deny connections from ahole ip addresses
	
		auto res = create_socket(conn_fd, false, &addr);
		if(res == NULL)
		{
			DEBUG_PRINTF("Error accepting connection\n");
			continue;
		}
	
		DEBUG_PRINTF("Connection successfully accepted, fd = %d\n", conn_fd);
	}
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
		DEBUG_PRINTF("Bad HTTP header, fd = %d\n", socket->socket_fd);
		dispose_socket(socket);
	}
	break;
	
	case HttpRequestType_Get:
	{
		DEBUG_PRINTF("Got cached HTTP request, fd = %d, url = %s\n", socket->socket_fd, socket->request.url.c_str());
		
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
		DEBUG_PRINTF("Got post HTTP request, fd = %d\n", socket->socket_fd);
		
		int total_size = socket->request.content_length + header_size;
		
		if(socket->request.content_length <= 0 || 
			socket->request.content_length + header_size > socket->inp.size)
		{
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
		DEBUG_PRINTF("Got a web socket connection, fd = %d\n", socket->socket_fd);
		
		//Make sure we read the extra 8 bytes for the connection key
		if((int)(socket->inp.buf_cur - socket->request.content_ptr) < 8)
		{
			if(!notify_socket(socket))
				dispose_socket(socket);
			return;
		}
		
		//Create the handshake packet
		socket->outp.init_buffer(SEND_BUFFER_SIZE);
		if(!http_websocket_handshake(socket->request, socket->outp.buf_start, &socket->outp.pending, config->readString("origin")))
		{
			DEBUG_PRINTF("Bad WebSocket handshake\n");
			dispose_socket(socket);
			return;
		}
		
		socket->state = SocketState_WebSocketHandshake;
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
	DEBUG_PRINTF("Sending cached reply, fd = %d, pending_bytes = %d\n", socket->socket_fd, socket->outp.pending);
	
	auto response = socket->cached_response->second;
	
	int offset = response.size - socket->outp.pending;
	int len = send(socket->socket_fd, response.buf + offset, socket->outp.pending, MSG_NOSIGNAL);
	
	if(len < 0)
	{
		if(errno == EAGAIN);
		{
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
		DEBUG_PRINTF("Remote end hung up during reply, fd = %d\n", socket->socket_fd);
		socket->cached_response.release();
		dispose_socket(socket);
		return;
	}
	
	socket->outp.pending -= len;

	if(socket->outp.pending <= 0)
	{
		DEBUG_PRINTF("Reply complete, fd = %d\n", socket->socket_fd);
		socket->cached_response.release();
		dispose_socket(socket);
		return;
	}
	
	if(!notify_socket(socket))
	{
		socket->cached_response.release();
		dispose_socket(socket);
		return;
	}
}

void HttpServer::process_recv(Socket* socket)
{
	DEBUG_PRINTF("Receiving post data, fd=%d,  pending_bytes = %d\n", socket->socket_fd, socket->inp.pending);
	
	int len = recv(socket->socket_fd, socket->inp.buf_cur, socket->inp.pending, 0);
	
	if(len < 0)
	{
		if(errno == EAGAIN);
		{
			notify_socket(socket);
			return;
		}
		perror("recv");
		dispose_socket(socket);
		return;
	}
	else if(len == 0)
	{
		DEBUG_PRINTF("Remote end hung up during recv, fd = %d\n", socket->socket_fd);
		dispose_socket(socket);
		return;
	}
	
	socket->inp.pending -= len;
	socket->inp.buf_cur += len;

	if(socket->inp.pending <= 0)
	{
		DEBUG_PRINTF("Recv complete, fd = %d\n", socket->socket_fd);
		
		switch(socket->state)
		{	
		case SocketState_PostRecv:
			process_post(socket);
			return;
		break;
		}

		dispose_socket(socket);		
		return;
	}
	
	if(!notify_socket(socket))
	{
		dispose_socket(socket);
		return;
	}
}

void HttpServer::process_send(Socket* socket)
{
	DEBUG_PRINTF("Sending, fd = %d, pending_bytes = %d\n", socket->socket_fd, socket->outp.pending);
	
	int len = send(socket->socket_fd, socket->outp.buf_cur, socket->outp.pending, MSG_NOSIGNAL);
	
	if(len < 0)
	{
		if(errno == EAGAIN);
		{
			notify_socket(socket);
			return;
		}
		perror("send");
		dispose_socket(socket);
		return;
	}
	else if(len == 0)
	{
		DEBUG_PRINTF("Remote end hung up during send, fd = %d\n", socket->socket_fd);
		dispose_socket(socket);
		return;
	}
	
	socket->outp.pending -= len;
	socket->outp.buf_cur += len;

	if(socket->outp.pending <= 0)
	{
		DEBUG_PRINTF("Send complete, fd = %d\n", socket->socket_fd);
		
		switch(socket->state)
		{	
		case SocketState_PostReply:
			DEBUG_PRINTF("Post reply complete, fd = %d\n", socket->socket_fd);
			dispose_socket(socket);
			return;
		break;
		
		case SocketState_WebSocketHandshake:
		{
			DEBUG_PRINTF("WebSocket handshake complete, fd = %d\n", socket->socket_fd);
			initialize_websocket(socket);
			return;
		}
		break;
		}
		
		assert(false);
		return;
	}
	
	if(!notify_socket(socket))
	{
		dispose_socket(socket);
		return;
	}
}


//Process a post request
void HttpServer::process_post(Socket* socket)
{
	//Parse client packet
	auto P = ScopeDelete<Network::ClientPacket>(new Network::ClientPacket());
	if(!P.ptr->ParseFromArray(socket->request.content_ptr, socket->request.content_length))
	{
		dispose_socket(socket);
		return;
	}
	
	
	auto reply = ScopeDelete<Network::ServerPacket>((*command_callback)(socket->request, P.ptr));
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
			DEBUG_PRINTF("EPOLL DIED!  OH SHIT THE THING IS GONNA CRASH! BAIL OUT!!!!");
			break;
		}
	
		for(int i=0; i<nfds; ++i)
		{
			Socket* socket = (Socket*)events[i].data.ptr;
			
			DEBUG_PRINTF("Processing event from socket: fd = %d\n", socket->socket_fd);
			
			if( (events[i].events & EPOLLHUP) ||
				(events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLRDHUP) )
			{
				DEBUG_PRINTF("Remote end hung up, fd = %d\n", socket->socket_fd);
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
				
				case SocketState_WebSocketHandshake:
				case SocketState_PostReply:
					process_send(socket);
				break;

				case SocketState_WebSocketRecv:
					handle_websocket_recv(socket);
				break;
				
				case SocketState_WebSocketSend:
					handle_websocket_send(socket);
				break;
			}
		}
	}
}

}
