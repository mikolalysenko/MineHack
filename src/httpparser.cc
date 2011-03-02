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

// HTTP Header parsing philosophy:
//
// Needs to accept output from our supported platforms and do it fast, and reject outright malicious 
// code.  Any extra cruft is ignored.
//
HttpHeader parse_http_header(char* ptr, char* end_ptr)
{
	HttpHeader result;
	result.type = HttpHeaderType_Bad;
	char c;
		  
	DEBUG_PRINTF("parsing header\n");
	
	//Check for no data case
	if(ptr == end_ptr)
		return result;
		  
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
	
	//For get stuff, we are done
	if(!post)
	{
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
			
		result.type = HttpHeaderType_Get;
		result.request = string(request_start, (int)(ptr - 1 - request_start));
		
		return result;
	}

	return result;
	
	/*

	DEBUG_PRINTF("Got post header, scanning for content-length\n");

	//Seek to content length field
	while(ptr < end_ptr)
	{
		c = *(ptr++);
		if(c == 'C')
		{
			//Check for field match
			const char ONTENT_LEN[] = "ontent-Length:";
			if(end_ptr - ptr < sizeof(ONTENT_LEN))
			{
				return -1;
			}
		
			bool match = true;
			for(int i=0; i<sizeof(ONTENT_LEN)-1; ++i)
			{
				if(ptr[i] != ONTENT_LEN[i])
				{
					match = false;
					break;
				}
			}
		
			if(match)
			{
				ptr += sizeof(ONTENT_LEN);
				break;
			}
		}
	}
	if(ptr == end_ptr)
		return -1;

	DEBUG_PRINTF("Found content length\n");

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
	content_length = atoi(content_str);

	DEBUG_PRINTF("Content length = %d\n", content_length);

	//Error: must specify content length > 0
	if(content_length <= 0)
		return 0;

	//Restore last character
	*ptr = c;
	
	DEBUG_PRINTF("Scanning for end of header\n");
	
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
		*(ptr-1) = '\0';
		
		DEBUG_PRINTF("Successfully parsed header: %s\n", recv_buf_start);
	
		content_ptr = ptr;
		
		return 1;
	}
	
	return -1;
	*/
}


/*

//Serializes a protocol buffer to an http response
char* serialize_protobuf_http(Network::ServerPacket& message, int* size)
{
	//Write object to buffer
	int buffer_len =  2 * message.ByteSize() + 32 + sizeof(DEFAULT_PROTOBUF_HEADER);
	char* buffer = (char*)malloc(buffer_len + 1);
	buffer[0] = '\n';	//Need to set to avoid messing up byte order mask
	if(!message.SerializeToArray(buffer + 1, buffer_len))
	{
		DEBUG_PRINTF("Protocol buffer serialize failed\n");
		free(buffer);
		return false;
	}
	
	//Compress serialized packet
	int compressed_size;
	ScopeFree compressed_buffer(tcgzipencode(buffer, message.ByteSize()+1, &compressed_size));
	if(compressed_buffer.ptr == NULL)
	{
		DEBUG_PRINTF("Protocol buffer GZIP compression failed\n");
		free(buffer);
		return false;
	}

	//Write packet offset
	int packet_offset = snprintf(buffer, buffer_len, DEFAULT_PROTOBUF_HEADER, compressed_size);
	if(packet_offset + compressed_size > buffer_len)
	{
		DEBUG_PRINTF("Send packet is too big\n");
		free(buffer);
		return false;
	}
	
	memcpy(buffer + packet_offset, compressed_buffer.ptr, compressed_size);
	return buffer;
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

