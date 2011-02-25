#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

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



HttpServer::HttpServer(Config* cfg, http_func cback) :
	config(cfg), callback(cback), cached_response(tcmapnew())
{
}

HttpServer::~HttpServer()
{
	clear_cache();
	tcmapdel(cached_response);
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


//Start the server
bool HttpServer::start()
{
	printf("Starting http server...\n");
	
	epollfd = -1;
	listen_socket = -1;
	living_threads = 0;
	listen_event = NULL;
	
	init_cache();
	
	{
		printf("Creating listen socket\n");
		
		//Create listen socket
		listen_socket = socket(PF_INET, SOCK_STREAM, 0);
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
    }
    
    {
    	printf("Creating epoll object\n");
    
		//Create epoll object
		epollfd = epoll_create(MAX_EPOLL_EVENTS);
		if(epollfd == -1)
		{
			perror("epoll_create");
			stop();
			return false;
		}
	
		//Add listen socket event
		listen_event = new HttpEvent();
		listen_event->type = HttpEventType::Connect;
		epoll_event event;
		event.events = EPOLLIN | EPOLLPRI;
		event.data.ptr = listen_event;
	
		if(epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_socket, &event) == -1)
		{
			perror("epoll_add");
			stop();
			return false;
		}
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
		printf("Closing epoll");
		close(epollfd);
		epollfd = -1;
	}
	
	if(listen_socket != -1)
	{
		printf("Closing listen socket");
		close(listen_socket);
		listen_socket = -1;
	}
	
	if(listen_event != NULL)
	{
		delete listen_event;
		listen_event = NULL;
	}
	
	//Join with remaining worker threads
	printf("Killing workers");
	for(int i=0; i<living_threads; ++i)
	{
		pthread_join(workers[i], NULL);
		pthread_detach(workers[i]);	
		printf("HTTP Worker %d killed\n", i);
	}
	living_threads = 0;
}

//Worker boot strap
void* HttpServer::worker_start(void* arg)
{
	((HttpServer*)arg)->worker_loop();
	pthread_exit(0);
	
	return NULL;
}

//Run the worker loop
void HttpServer::worker_loop()
{
	while(running)
	{
		sched_yield();
	}
}

