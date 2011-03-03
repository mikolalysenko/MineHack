#ifndef HTTPPARSER_H
#define HTTPPARSER_H

#include <map>
#include <string>
#include <tbb/concurrent_hash_map.h>

namespace Game
{
	enum HttpRequestType
	{
		HttpRequestType_Bad,
		HttpRequestType_Get,
		HttpRequestType_Post,
		HttpRequestType_WebSocket
	};

	struct HttpRequest
	{
		HttpRequestType type;
		std::string request;
		std::map<std::string, std::string> headers;
	};
	
	//An http response
	struct HttpResponse
	{
		int size;
		char* buf;
	};

	//Parses an http header
	HttpRequest parse_http_request(char* buf_start, char* buf_end);
	
	//Caches a directory
	void cache_directory(tbb::concurrent_hash_map<std::string, HttpResponse>& cache, std::string const& wwwroot);
};

#endif
