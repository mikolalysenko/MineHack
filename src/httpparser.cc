#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#include <map>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>

#include "stdint.h"

#include "constants.h"
#include "misc.h"
#include "httpparser.h"

//Uncomment this line to get dense logging for the web server
#define HTTP_DEBUG 1

#ifndef HTTP_DEBUG
#define DEBUG_PRINTF(...)
#else
#define DEBUG_PRINTF(...)  fprintf(stderr,__VA_ARGS__)
#endif

using namespace std;
using namespace tbb;


namespace Game {

const char DEFAULT_ERROR_RESPONSE[] = 
	"HTTP/1.1 403 Forbidden\r\n\r\n";

const char DEFAULT_HTTP_HEADER[] =  
	"HTTP/1.1 200 OK\r\n"
	"Content-Encoding: gzip\r\n"
	"Content-Length: %d\r\n\r\n";
	
const char DEFAULT_PROTOBUF_HEADER[] =
	"HTTP/1.1 200 OK\r\n"
	"Cache: no-cache\r\n"
	"Content-Type: application/octet-stream; charset=x-user-defined\r\n"
	"Content-Transfer-Encoding: binary\r\n"
	"Content-Encoding: gzip\r\n"
	"Content-Length: %d\r\n"
	"\r\n";
	
const char DEFAULT_WEBSOCKET_HEADER[] = 
	"HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
	"Upgrade: WebSocket\r\n"
	"Connection: Upgrade\r\n"	
	"Sec-WebSocket-Origin: http://%s\r\n"
	"Sec-WebSocket-Location: ws://%s/%s\r\n"
	"Sec-WebSocket-Protocol: pbuf\r\n"
	"\r\n";

//FIXME: This should get read from the config
const char ORIGIN[] = "127.0.0.1:8081";

void parse_http_request(char* ptr, char* end_ptr, HttpRequest& result)
{
	result.type = HttpRequestType_Bad;
	result.content_ptr = end_ptr;
	char c;
		  
	DEBUG_PRINTF("parsing header\n");
	
	//Check for no data case
	if(ptr == end_ptr)
		return;
		  
	//Eat leading whitespace
	for(c = *ptr;
		(c == ' ' || c == '\r' || c == '\n' || c == '\t') && ptr < end_ptr;
		c = *(++ptr))
	{	
	}
		  
	int len = (int)(end_ptr - ptr);
	if(len < 12)
		return;
	
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
		DEBUG_PRINTF("Unkown HTTP request type\n");
		return;
	}
	
	//Skip leading slash
	if(*ptr == '/')
	{
		ptr++;	
	}
	
	//Parse out the request portion
	char* request_start = ptr;

	//Read to end of request
	while(ptr < end_ptr)
	{
		c = *(ptr++);
		if(c == ' ' || c == '\0')
			break;
	}

	//Set end of request
	if(ptr == end_ptr)
		return;
		
	result.url = string(request_start, (int)(ptr - 1 - request_start));
	
	//Scan to end of line
	while(ptr < end_ptr)
	{
		c = *(ptr++);
		if(c == '\n')
			break;
	}
	if(ptr == end_ptr)
		return;
		
	//Split out the header fields
	while(ptr < end_ptr)
	{
		//Eat whitespace
		for(c = *ptr;
			(c == ' ' || c == '\r' || c == '\n' || c == '\t') && ptr < end_ptr;
			c = *(++ptr))
		{	
		}
		if(ptr == end_ptr)
			break;
	
		char* header_start = ptr;
	
		//Scan in request field
		while(ptr < end_ptr)
		{
			c = *(ptr++);
			if(c == ':')
				break;
		}
		if(ptr == end_ptr)
			break;
		
		string request_header_name = string(header_start, ptr-1);
		
		//Skip the ': '
		ptr ++;
		if(ptr >= end_ptr)
			break;
		
		//Scan to end of line
		char* data_start = ptr;
		while(ptr < end_ptr)
		{
			c = *(ptr++);
			if(c == '\r' || c == '\n')
				break;
		}
		if(ptr == end_ptr)
			break;
		
		string request_data = string(data_start, ptr-1);
		
		DEBUG_PRINTF("Got request header: %s, '%s'\n", request_header_name.c_str(), request_data.c_str());
		
		result.headers[request_header_name] = request_data;
	}
	
	if(post)
	{
		auto iter = result.headers.find("Content-Length");
		if(iter == result.headers.end())
		{
			DEBUG_PRINTF("Missing content length field in post request\n");
			return;
		}
		
		result.content_length = atoi(iter->second.c_str());
		
		if(result.content_length >= 0)
		{
			result.type = HttpRequestType_Post;
		}
	}
	else
	{
		auto iter = result.headers.find("Upgrade");
		
		if(iter != result.headers.end() && iter->second == "WebSocket")
		{
			result.type = HttpRequestType_WebSocket;
		}
		else
		{
			result.type = HttpRequestType_Get;
		}
	}
}


//Serializes a protocol buffer to an http response
HttpResponse http_serialize_protobuf(Network::ServerPacket* message)
{
	HttpResponse result;
	result.buf = NULL;
	result.size = 0;

	//Write object to buffer
	int bs = message->ByteSize();
	int buffer_len =  2 * bs + 32 + sizeof(DEFAULT_PROTOBUF_HEADER);
	char* buffer = (char*)malloc(buffer_len + 1);
	buffer[0] = '\n';	//Need to set to avoid messing up byte order mask
	if(!message->SerializeToArray(buffer + 1, buffer_len))
	{
		DEBUG_PRINTF("Protocol buffer serialize failed\n");
		free(buffer);
		return result;;
	}
	
	//Compress serialized packet
	int compressed_size;
	ScopeFree compressed_buffer(tcgzipencode(buffer, bs+1, &compressed_size));
	if(compressed_buffer.ptr == NULL)
	{
		DEBUG_PRINTF("Protocol buffer GZIP compression failed\n");
		free(buffer);
		return result;
	}

	//Write packet offset
	int packet_offset = snprintf(buffer, buffer_len, DEFAULT_PROTOBUF_HEADER, compressed_size);
	if(packet_offset + compressed_size > buffer_len)
	{
		DEBUG_PRINTF("Send packet is too big\n");
		free(buffer);
		return result;
	}
	
	memcpy(buffer + packet_offset, compressed_buffer.ptr, compressed_size);
	
	result.size = compressed_size + packet_offset;
	result.buf = buffer;
	
	return result;
}

int64_t compute_ws_key(string const& key)
{
	uint64_t num = 0, n_spaces = 0;
	
	for(int i=0; i<key.size(); ++i)
	{
		uint8_t c = key[i];
		
		if('0' <= c && c <= '9')
		{
			num = (num * 10) + (c - '0');
		}
		else if(c == ' ')
		{
			n_spaces++;
		}
	}
	
	DEBUG_PRINTF("Num spaces = %ld, key base = %ld\n", n_spaces, num);
	
	if(n_spaces == 0)
		return -1;
	return num / n_spaces;
}


int hex_to_num(uint8_t c)
{
	if(c >= '0' && c <= '9')
		return c - '0';
		
	if(c >= 'a' && c <= 'f')
		return c - 'a';
	
	if(c >= 'A' && c <= 'F')
		return c - 'A';
		
	return 0;
}

//Generates a websocket handshake reply
bool http_websocket_handshake(HttpRequest const& request, char* buf, int* size)
{
	int header_size = sprintf(buf, DEFAULT_WEBSOCKET_HEADER, ORIGIN, ORIGIN, request.url.c_str());
	*size = 16 + header_size;

	//Compute the request hash for each private key
	DEBUG_PRINTF("Checking key 1\n");
	auto iter1 = request.headers.find("Sec-WebSocket-Key1");	
	if(iter1 == request.headers.end())
	{
		DEBUG_PRINTF("Missing key 1\n");
		return false;
	}
	int64_t k1 = compute_ws_key(iter1->second);
	if(k1 < 0)
		return false;
	DEBUG_PRINTF("Key 1 = %ld\n", k1);
		
	
	DEBUG_PRINTF("Checking key 2\n");
	auto iter2 = request.headers.find("Sec-WebSocket-Key2");	
	if(iter2 == request.headers.end())
	{
		DEBUG_PRINTF("Missing key 2\n");
		return false;
	}
	int64_t k2 = compute_ws_key(iter2->second);
	if(k2 < 0)
		return false;
	DEBUG_PRINTF("Key 2 = %ld\n", k2);
	
	DEBUG_PRINTF("Key 3 = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
		(uint8_t)request.content_ptr[0],
		(uint8_t)request.content_ptr[1],
		(uint8_t)request.content_ptr[2],
		(uint8_t)request.content_ptr[3],
		(uint8_t)request.content_ptr[4],
		(uint8_t)request.content_ptr[5],
		(uint8_t)request.content_ptr[6],
		(uint8_t)request.content_ptr[7]);
	
	//Copy the buffer
	char buffer[16];
	*((uint32_t*)buffer) = (uint32_t)k1;
	*((uint32_t*)(buffer+4)) = (uint32_t)k1;
	memcpy(buffer+8, request.content_ptr, 8);
		
	//This is kind of dumb, but whatever
	char md5[64];
	tcmd5hash(buffer, 16, md5);
	uint8_t *mptr = (uint8_t*)(buf + header_size);
	for(int i=0; i<16; ++i)
	{
		char h = md5[2*i], l = md5[2*i+1];
		*(mptr++) = hex_to_num(h)*16 + hex_to_num(l);
	}
	
	return true;
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
void cache_directory(concurrent_hash_map<string, HttpResponse>& cache, string const& wwwroot)
{

	DEBUG_PRINTF("Initializing cache\n");
	
	vector<string> files;
	list_files(wwwroot, files);

	for(int i=0; i<files.size(); ++i)
	{
		//Read file into memory
		int raw_size;
		ScopeFree raw_data(read_file(files[i], &raw_size));
		
		if(raw_data.ptr == NULL)
		{
			DEBUG_PRINTF("\tError reading file: %s\n", files[i].c_str());
			continue;
		}
		DEBUG_PRINTF("\tCaching file: %s\n", files[i].c_str());
		
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
		HttpResponse http_response;
		http_response.size = response_len;
		http_response.buf = (char*)response.ptr;
		response.ptr = NULL;		
		
		concurrent_hash_map<string, HttpResponse>::accessor acc;
		if(cache.find(acc, files[i]))
		{
			free(acc->second.buf);
			acc->second = http_response;
			acc.release();
		}
		else
		{
			cache.insert(make_pair(files[i], http_response));
		}
	}
}

};
