#ifndef HTTPPARSER_H
#define HTTPPARSER_H

#include <string>
#include <tbb/concurrent_hash_map.h>

namespace Game
{
	enum HttpHeaderType
	{
		HttpHeaderType_Bad,
		HttpHeaderType_Get,
		HttpHeaderType_Post,
		HttpHeaderType_WebSocket
	};

	struct HttpHeader
	{
		HttpHeaderType type;
		
		//The request string
		std::string request;
		
		//Content pointers for post data
		int content_len;
		char* content_ptr;
		
		//FIXME: Add websocket stuff here
	};
	
	//An http response
	struct HttpResponse
	{
		int size;
		char* buf;
	};

	//Parses an http header
	HttpHeader parse_http_header(char* buf_start, char* buf_end);
	
	//Caches a directory
	void cache_directory(tbb::concurrent_hash_map<std::string, HttpResponse>& cache, std::string const& wwwroot);
};

#endif
