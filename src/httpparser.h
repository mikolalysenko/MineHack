#ifndef HTTPPARSER_H
#define HTTPPARSER_H

#include <map>
#include <string>
#include <tbb/concurrent_hash_map.h>

#include "constants.h"
#include "network.pb.h"

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
		std::string url;
		std::map<std::string, std::string> headers;
		
		//For post requests
		int content_length;
		char* content_ptr;
	};
	
	//An http response
	struct HttpResponse
	{
		int size;
		char* buf;
	};

	//Parses an http header
	void parse_http_request(char* buf_start, char* buf_end, HttpRequest&);
	
	//Generates a protocol buffer base http serialized packet
	HttpResponse http_serialize_protobuf(Network::ServerPacket* message);
	
	//Generates a web socket handshake accept
	HttpResponse http_websocket_handshake(HttpRequest const& request);
	
	//Caches a directory
	void cache_directory(tbb::concurrent_hash_map<std::string, HttpResponse>& cache, std::string const& wwwroot);
};

#endif
