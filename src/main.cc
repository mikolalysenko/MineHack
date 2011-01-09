#include <iostream>
#include <sstream>
#include <cassert>
#include <cstring>

#include "mongoose.h"
#include "login.h"
#include "session.h"
#include "misc.h"
#include "world.h"

using namespace std;
using namespace Server;
using namespace Game;

static const char *options[] = {
  "document_root", "www",
  "listening_ports", "8081",
  "num_threads", "5",
  NULL
};

static const char *ajax_reply_start =
  "HTTP/1.1 200 OK\n"
  "Cache: no-cache\n"
  "Content-Type: application/x-javascript\n"
  "\n";



World * game_instance = NULL;

//Server initialization
void init()
{
	init_login();
	init_sessions();
	
	game_instance = new World();
}

int get_int(const char* id, const mg_request_info *req)
{
	char str[16];
	
	const char* qs = req->query_string;
	size_t qs_len = (qs == NULL ? 0 : strlen(qs));
	
	mg_get_var(qs, qs_len, id, str, 16);
	
	stringstream ss(str);
	int res;
	ss >> res;
	
	return res;
}


void ajax_send_binary(mg_connection *conn, const void* buf, size_t len)
{
	mg_printf(conn,
	  "HTTP/1.1 200 OK\n"
	  "Cache: no-cache\n"
	  "Content-Type: application/octet-stream; charset=utf-8\n"
	  "Content-Transfer-Encoding: binary\n"
	  "Content-Length: %d\n"
	  "\n", len);

	mg_write(conn, buf, len);	

}

//Retrieves the session id
bool get_session_id(const mg_request_info* request_info, SessionID& res)
{
	char session_id_str[128];
	
	const char* qs = request_info->query_string;
	size_t qs_len = (qs == NULL ? 0 : strlen(qs));
	
	mg_get_var(qs, qs_len, "k", session_id_str, 127);
	
	if(session_id_str[0] == '\0')
		return false;
	
	stringstream ss(session_id_str);
	ss >> res;
	
	if(valid_session_id(res))
		return true;
	
	return true;
}

//Handle a user login
void do_login(
	mg_connection* conn, 
	const mg_request_info* request_info)
{
	char user_name[256];
	char password_hash[256];
	
	const char* qs = request_info->query_string;
	size_t qs_len = (qs == NULL ? 0 : strlen(qs));
	
	mg_get_var(qs, qs_len, "n", user_name, 256);
	mg_get_var(qs, qs_len, "p", password_hash, 256);
	
	SessionID key;
	
	cout << "got login: " << user_name << ", pwd hash = " << password_hash << endl;
	
	if(verify_user_name(user_name, password_hash) && create_session(user_name, key))
	{
		stringstream ss;
		ss << key;
	
		mg_printf(conn, "%sOk\n%s", ajax_reply_start, ss.str().c_str());
	}
	else
	{
		mg_printf(conn, "%sFail\nCould not log in", ajax_reply_start);
	}
}

//Create an account
void do_register(
	mg_connection* conn, 
	const mg_request_info* request_info)
{
	char user_name[256];
	char password_hash[256];
	
	const char* qs = request_info->query_string;
	size_t qs_len = (qs == NULL ? 0 : strlen(qs));
	
	mg_get_var(qs, qs_len, "n", user_name, 256);
	mg_get_var(qs, qs_len, "p", password_hash, 256);
	
	if(user_name[0] == '\0' ||
		password_hash[0] == '\0')
	{
		mg_printf(conn, "%sFail\nMissing username/password", ajax_reply_start);
	}
	else if(create_account(string(user_name), string(password_hash)))
	{
		do_login(conn, request_info);
	}
	else
	{
		mg_printf(conn, "%sFail\nUser name already in use", ajax_reply_start);	
	}
}

//Handle user log out
void do_logout(
	mg_connection* conn, 
	const mg_request_info* request_info)
{
	SessionID session_id;
	if(get_session_id(request_info, session_id))
	{
		delete_session(session_id);
	}
	mg_printf(conn, "%s\nOk", ajax_reply_start);
}


//Retrieves a chunk
void do_get_chunk(
	mg_connection* conn,
	const mg_request_info* request_info)
{
	//Check session id here
	
	//Extract the chunk index here
	ChunkId idx;
	idx.x = get_int("x", request_info);
	idx.y = get_int("y", request_info);
	idx.z = get_int("z", request_info);
	
	cout << "chunk index = " << idx.x << "," << idx.y << "," << idx.z << endl;
	auto chunk = game_instance->game_map->get_chunk(idx);	
	printf("chunk = %08x\n", chunk);
	
	if(chunk == NULL)
	{
		mg_printf(conn, "%s", ajax_reply_start);
		return;
	}

	const int BUF_LEN = 32*32*32*2;
	uint8_t chunk_buf[BUF_LEN];
	int len = chunk->compress(chunk_buf, BUF_LEN);
	
	printf("len = %d\n", len);
	if(len < 0)
	{
		mg_printf(conn, "%s", ajax_reply_start);
		return;
	}
	
	ajax_send_binary(conn, (const void*)chunk_buf, len);	
}

//Event dispatch
static void *event_handler(enum mg_event event,
                           struct mg_connection *conn,
                           const struct mg_request_info *request_info)
{

	if (event == MG_NEW_REQUEST)
	{
		cout << "Got request: " << request_info->uri << endl;
	
		if(strcmp(request_info->uri, "/l") == 0)
		{ do_login(conn, request_info);
		}
		else if(strcmp(request_info->uri, "/r") == 0)
		{ do_register(conn, request_info);
		}
		else if(strcmp(request_info->uri, "/q")	 == 0)
		{ do_logout(conn, request_info);
		}
		else if(strcmp(request_info->uri, "/g") == 0)
		{ do_get_chunk(conn, request_info);
		}
		else
		{ return NULL;
		}
		
		return (void*)"yes";
	}
	
	return NULL;
}


int main(int argc, char** argv)
{
	init();

	mg_context *context = mg_start(&event_handler, options);
	assert(context != NULL);

	// Wait until enter is pressed, then exit
	cout << "Server started" << endl;
	int c;
	cin >> c;
	mg_stop(context);
	cout << "Server stopped" << endl;

	return 0;
}

