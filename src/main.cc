#include <unistd.h>
#include <fcntl.h>

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

void ajax_ok(mg_connection* conn)
{
	mg_printf(conn, "%s", ajax_reply_start);
}

void ajax_error(mg_connection* conn)
{
	mg_printf(conn, "HTTP/1.1 403 Forbidden\nCache: no-cache\n\n");
}

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

//Pushes a binary message to the client
void ajax_send_binary(mg_connection *conn, const void* buf, size_t len)
{
	mg_printf(conn,
	  "HTTP/1.1 200 OK\n"
	  "Cache: no-cache\n"
	  "Content-Type: application/octet-stream; charset=x-user-defined\n"
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
	
	return false;
}

//Handle a user login
void do_login(
	mg_connection* conn, 
	const mg_request_info* request_info)
{
	char user_name[32];
	char password_hash[256];
	
	
	const char* qs = request_info->query_string;
	size_t qs_len = (qs == NULL ? 0 : strlen(qs));
	
	memset(user_name, 0, 32);
	mg_get_var(qs, qs_len, "n", user_name, 20);
	mg_get_var(qs, qs_len, "p", password_hash, 256);
	
	cout << "got login: " << user_name << ", pwd hash = " << password_hash << endl;
	
	//Retrieve session id and verify user name
	SessionID key;
	if(verify_user_name(user_name, password_hash) && create_session(user_name, key))
	{
		//Push login event to server
		InputEvent ev;
		ev.type = InputEventType::PlayerJoin;
		ev.session_id = key;
		memcpy(ev.join_event.name, user_name, 32);
		game_instance->add_event(ev);
	
		
		//Debug spam
		cout << "Logged in: " << key << endl;
	
		//Send key to client
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
		
		InputEvent ev;
		ev.type = InputEventType::PlayerLeave;
		ev.session_id = session_id;
		game_instance->add_event(ev);
	}
	mg_printf(conn, "%s\nOk", ajax_reply_start);
}


//Retrieves a chunk
void do_get_chunk(
	mg_connection* conn,
	const mg_request_info* request_info)
{
	//Check session id here
	SessionID session_id;
	if(!get_session_id(request_info, session_id))
	{
		cout << "Invalid session" << endl;
	
		ajax_error(conn);
		return;
	}
	
	//Extract the chunk index here
	ChunkID idx;
	idx.x = get_int("x", request_info);
	idx.y = get_int("y", request_info);
	idx.z = get_int("z", request_info);
	
	//Extract the chunk
	const int BUF_LEN = 32*32*32*2;
	uint8_t chunk_buf[BUF_LEN];
	int len = game_instance->get_compressed_chunk(session_id, idx, chunk_buf, BUF_LEN);
		
	if(len < 0)
	{
		ajax_error(conn);
	}
	else
	{
		ajax_send_binary(conn, (const void*)chunk_buf, len);	
	}
}


//Pulls pending events from client
void do_heartbeat(
	mg_connection* conn,
	const mg_request_info* request_info)
{
	//Check session id
	SessionID session_id;
	if(!get_session_id(request_info, session_id))
	{
		cout << "Invalid session" << endl;
		ajax_error(conn);
		return;
	}
	
	//Unpack incoming events
	const int BUF_LEN = (1<<16);
	uint8_t msg_buf[BUF_LEN];
	memset(msg_buf, '\0', BUF_LEN);
	int len = mg_read(conn, msg_buf, BUF_LEN);
	
	//Parse out the events
	int p = 0;
	while(true)
	{
		//Create event
		InputEvent ev;
		ev.session_id = session_id;
		
		int d = ev.extract(&msg_buf[p], len);
		if(d < 0)
			break;
		
		//Add event
		game_instance->add_event(ev);
		p += d;
		len -= d;
	}
	//Generate client response
	len = game_instance->heartbeat(session_id, msg_buf, BUF_LEN);
	
	cout << "Heartbeat Len = " << len << endl;
	
	if(len >= 0)
	{
		if(len > 0)
		{
			cout << "Heartbeat Response: ";
			for(int i=0; i<len; i++)
				cout << (int)msg_buf[i] << ',';
			cout << endl;
				
		}
	
		ajax_send_binary(conn, msg_buf, len);
	}
	else
	{
		ajax_error(conn);
	}
}

//Event dispatch
static void *event_handler(enum mg_event event,
                           struct mg_connection *conn,
                           const struct mg_request_info *request_info)
{

	if (event == MG_NEW_REQUEST)
	{
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
		else if(strcmp(request_info->uri, "/h") == 0)
		{ do_heartbeat(conn, request_info);
		}
		else
		{ return NULL;
		}
		
		return (void*)"yes";
	}
	
	return NULL;
}

//Handles a message from the server command line
void handle_message(const char* str, size_t len)
{
	if(strcmp(str, "q") == 0)
	{
		//Quit!
		game_instance->running = false;
		cout << "Stopping server" << endl;
	}
	else
	{
		cout << "Unknown server command." << endl;
	}
}

//The main server loop
void loop()
{
	const size_t BUF_LEN = 1024;
	char msg_buf[BUF_LEN];
	int buf_ptr = 0;
	
	//Set stdin to non-blocking
	fcntl(0, F_SETFL, O_NONBLOCK);
	
	while(game_instance->running)
	{
		game_instance->tick();
		
		//Need to do this to avoid blocking (is there a better way?)
		while(true)
		{
			int c = getc(stdin);
			if(c == EOF)
				break;
			
			if(buf_ptr >= BUF_LEN-1 || c == '\n')
			{
				msg_buf[buf_ptr] = '\0';
				handle_message(msg_buf, buf_ptr);
				buf_ptr = 0;
			}
			else
				msg_buf[buf_ptr++] = c;
		}
	}
}

//Program start point
int main(int argc, char** argv)
{
	init();
	
	//Start web server
	mg_context *context = mg_start(&event_handler, options);
	assert(context != NULL);
	cout << "Server started" << endl;
	
	//Run main thread
	loop();
	
	//Stop web server
	mg_stop(context);
	cout << "Server stopped" << endl;

	return 0;
}

