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

HttpRequest parse_http_request(char* ptr, char* end_ptr)
{
	HttpRequest result;
	result.type = HttpRequestType_Bad;
	char c;
		  
	DEBUG_PRINTF("parsing header\n");
	
	//Check for no data case
	if(ptr == end_ptr)
		return result;
		  
	//Eat leading whitespace
	for(c = *ptr;
		(c == ' ' || c == '\r' || c == '\n' || c == '\t') && ptr < end_ptr;
		c = *(++ptr))
	{	
	}
		  
	int len = (int)(end_ptr - ptr);
	if(len < 12)
		return result;
	
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
		return result;
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
		return result;
		
	result.request = string(request_start, (int)(ptr - 1 - request_start));
	
	//Scan to end of line
	while(ptr < end_ptr)
	{
		c = *(ptr++);
		if(c == '\n')
			break;
	}
	if(ptr == end_ptr)
		return result;
		
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
		
		//Eat more whitespace
		for(c = *ptr;
			(c == ' ' || c == '\t') && ptr < end_ptr;
			c = *(++ptr))
		{	
		}
		if(ptr == end_ptr)
			break;
		
		//Scan to 
		char* data_start = ptr;
		while(ptr < end_ptr)
		{
			c = *(ptr++);
			if(c == '\n' || c == '\r')
				break;
		}
		if(ptr == end_ptr)
			break;
		
		string request_data = string(data_start, ptr-1);
		
		DEBUG_PRINTF("Got request header: %s, %s\n", request_header_name.c_str(), request_data.c_str());
		
		result.headers[request_header_name] = request_data;
	}
	
	//Check for web socket
	
	if(post)
	{
		result.type = HttpRequestType_Post;
	}
	else
	{
		result.type = HttpRequestType_Get;
	}
	
	return result;
}


/*
//Serializes a protocol buffer to an http response
HttpResponse http_serialize_protobuf(Network::ServerPacket& message)
{
	HttpResponse result;
	result.buf = NULL;
	result.size = 0;

	//Write object to buffer
	int buffer_len =  2 * message.ByteSize() + 32 + sizeof(DEFAULT_PROTOBUF_HEADER);
	char* buffer = (char*)malloc(buffer_len + 1);
	buffer[0] = '\n';	//Need to set to avoid messing up byte order mask
	if(!message.SerializeToArray(buffer + 1, buffer_len))
	{
		DEBUG_PRINTF("Protocol buffer serialize failed\n");
		free(buffer);
		return result;;
	}
	
	//Compress serialized packet
	int compressed_size;
	ScopeFree compressed_buffer(tcgzipencode(buffer, message.ByteSize()+1, &compressed_size));
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

*/

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

