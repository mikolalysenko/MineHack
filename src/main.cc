#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <sstream>
#include <cassert>
#include <cstring>
#include <cstdarg>
#include <cstdio>

#include "mongoose.h"
#include "login.h"
#include "session.h"
#include "misc.h"
#include "world.h"
#include "inventory.h"

using namespace std;
using namespace Server;
using namespace Game;

//Size of an integer query variable
#define INT_QUERY_LEN		32

//Length of the console command buffer
#define COMMAND_BUFFER_LEN	256

//Maximum size for an event packet
#define EVENT_PACKET_SIZE	(1<<16)

static const char *options[] = {
  "document_root", "www",
  "listening_ports", "8081",
  "num_threads", "5",
  NULL
};

World * game_instance = NULL;

//Event data
struct HttpEvent
{
	mg_connection* conn;
	const mg_request_info* req;
};

// --------------------------------------------------------------------------------
//A client session has 3 states:
//
// 	Not logged in - before a session id has been created, user has not signed on to account
//	Character select - user has logged in, but is not yet in the game world
//	In game		- user has joined game, can participate in the game world.
//
//	Event handlers are grouped according to which state they act
// --------------------------------------------------------------------------------

// ----------------- Login/account event handlers -----------------
void do_register(HttpEvent&);
void do_login(HttpEvent&);
void do_logout(HttpEvent&);

// ----------------- Session / character event handleers -----------------
void do_create_player(HttpEvent&);
void do_join_game(HttpEvent&);
void do_list_players(HttpEvent&);
void do_delete_player(HttpEvent&);

// ----------------- Game event handlers -----------------
void do_heartbeat(HttpEvent&);
void do_get_chunk(HttpEvent&);



// --------------------------------------------------------------------------------
//    Client output methods
// --------------------------------------------------------------------------------

//Sends an error message to a particular connection
void ajax_error(mg_connection* conn) 
{
	mg_printf(conn, "HTTP/1.1 403 Forbidden\nCache: no-cache\n\n");
}

//Prints a message to the target with the ajax header
void ajax_printf(mg_connection* conn, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
		
	//Write header
	mg_printf(conn, 
	  "HTTP/1.1 200 OK\n"
	  "Cache: no-cache\n"
	  "Content-Type: application/x-javascript\n"
	  "\n");

	//Get string length and allocate buffer
	int len = vsprintf(NULL, fmt, args);	
	char* buf = (char*)malloc(len);
	
	//Set guard
	ScopeFree guard(buf);
	
	//Format string and write to web socket
	vsprintf(NULL, fmt, args);
	mg_write(conn, buf, len);
}

//Sends a binary blob to the client
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

// --------------------------------------------------------------------------------
//    Client input methods
// --------------------------------------------------------------------------------

//Retrieves a string
bool get_string(
	const mg_request_info* request_info, 
	const std::string& var,
	std::string& res)
{
	char tmp_str[256];
	
	const char* qs = request_info->query_string;
	size_t qs_len = (qs == NULL ? 0 : strlen(qs));
	
	int l = mg_get_var(qs, qs_len, var.c_str(), tmp_str, 256);
	
	if(l < 0)
		return false;
		
	res = tmp_str;
	return true;
}

// Retrieves the session id from a URL
bool get_session_id(
	const mg_request_info* request_info, 
	SessionID& session_id)
{
	//Allocate a buffer
	char session_id_str[SESSION_ID_STR_LEN];	
	memset(session_id_str, 0, sizeof(session_id_str));
	
	const char* qs = request_info->query_string;
	size_t qs_len = (qs == NULL ? 0 : strlen(qs));
	
	int len = mg_get_var(qs, qs_len, "k", session_id_str, SESSION_ID_STR_LEN);
	
	if(len < 0)
		return false;
	
	//Scan in the session id
	sscanf(session_id_str, "%lx", &session_id.id);
	
	if(valid_session_id(session_id))
		return true;
	
	return false;
}

//The HttpBlob reader just pulls all the text out of a body in binary form
struct HttpBlobReader
{
	int len;
	void* data;
	
	HttpBlobReader(mg_connection* conn) : len(-1), data(NULL)
	{
		//Read number of available bytes
		int avail = mg_available_bytes(conn);
		if(avail < 0)
		{	avail = -1;
			return;
		}
		
		//Allocate buffer
		data = malloc(avail);
		len = mg_read(conn, data, avail);
		
		//Sanity check
		assert(len == avail);
	}
	
	//Free data
	~HttpBlobReader()
	{	
		if(data != NULL)
			free(data);
	}
};



// --------------------------------------------------------------------------------
//    Prelogin event handlers
// --------------------------------------------------------------------------------

//Create an account
void do_register(HttpEvent& ev)
{
	string username, password_hash;
	
	if(	!get_string(ev.req, "n", username) ||
		!get_string(ev.req, "p", password_hash) )
	{
		ajax_printf(ev.conn, 
			"Fail\n"	
			"Missing username/password\n");
	}
	else if(create_account(username, password_hash))
	{
		do_login(ev);
	}
	else
	{
		ajax_printf(ev.conn,
			"Fail\n"
			"Username already in use\n");
	}
}


//Handle a user login
void do_login(HttpEvent& ev)
{
	string username, password_hash;
	
	if(	!get_string(ev.req, "n", username) ||
		!get_string(ev.req, "p", password_hash) )
	{
		ajax_printf(ev.conn, 
			"Fail\n"	
			"Missing username/password\n");
		
		return;
	}

	SessionID session_id;
	if( verify_username(username, password_hash) &&
		create_session(username, session_id) )
	{
		ajax_printf(ev.conn, 
			"Ok\n"
			"%016lx\n", session_id.id);
	}
	else
	{
		ajax_printf(ev.conn,
			"Fail\n"
			"Could not log in\n");	
	}
}


//Handle user log out
void do_logout(HttpEvent& ev)
{
	SessionID session_id;
	if(get_session_id(ev.req, session_id))
	{
		delete_session(session_id);
	}

	ajax_printf(ev.conn, "Ok");
}



// --------------------------------------------------------------------------------
//    Character select event handlers
// --------------------------------------------------------------------------------

void do_create_player(HttpEvent& ev)
{
}

void do_join_game(HttpEvent& ev)
{
}

void do_list_players(HttpEvent& ev)
{
}

void do_delete_player(HttpEvent& ev)
{
}



// --------------------------------------------------------------------------------
//    Game event handlers
// --------------------------------------------------------------------------------

//Retrieves a chunk
void do_get_chunk(HttpEvent& ev)
{
	//Check session id here
	SessionID session_id;
	if(!get_session_id(ev.req, session_id))
	{
		cout << "Invalid session" << endl;
		ajax_error(ev.conn);
		return;
	}
	
	/*
	//Extract the chunk index here
	ChunkID idx;
	idx.x = get_int("x", request_info);
	idx.y = get_int("y", request_info);
	idx.z = get_int("z", request_info);
	
	//Extract the chunk
	uint8_t chunk_buf[MAX_CHUNK_BUFFER_LEN];
	int len = game_instance->get_compressed_chunk(session_id, idx, chunk_buf, MAX_CHUNK_BUFFER_LEN);
		
	if(len < 0)
	{
		ajax_error(conn);
	}
	else
	{
		cout << "Sending chunk (" << idx.x << ',' << idx.y << ',' << idx.z << "): ";
		for(int i=0; i<len; i++)
			cout << (int)chunk_buf[i] << ',';
		cout << endl;
	
		ajax_send_binary(conn, (const void*)chunk_buf, len);	
	}
	*/
	
	
}


//Pulls pending events from client
void do_heartbeat(HttpEvent& ev)
{

	/*
	//Check session id
	SessionID session_id;
	if(!get_session_id(request_info, session_id))
	{
		cout << "Invalid session" << endl;
		ajax_error(conn);
		return;
	}
	
	{	//Unpack incoming events
		uint8_t msg_buf[EVENT_PACKET_SIZE];
		memset(msg_buf, 0, EVENT_PACKET_SIZE);
		int len = mg_read(conn, msg_buf, EVENT_PACKET_SIZE);
	
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
	
		//This shouldn't happen, but need to check anyway
		if(len > 0)
		{
			cout << "Warning!  Unused data in heartbeat buffer: ";
			for(int i=0; i<len; i++)
			{
				cout << (int)msg_buf[p+i] << ",";
			}
			cout << endl;
		}
	}
	
	{	//Generate client response
		int mlen;
		void * data = game_instance->heartbeat(session_id, mlen);
	
		if(data == NULL)
		{
			ajax_send_binary(conn, data, 0);
			return;
		}
		
		ScopeFree guard(data);

		if(mlen > 0)
		{
			cout << "Pushing update packet: ";
			for(int i=0; i<mlen; i++)
				cout << (int)((uint8_t*)data)[i] << ',';
			cout << endl;
		}

		ajax_send_binary(conn, data, mlen);
	}
	*/
}


// --------------------------------------------------------------------------------
//    Server interface code
// --------------------------------------------------------------------------------

//The event dispatcher
static void *event_handler(mg_event event,
                           mg_connection *conn,
                           const mg_request_info *request_info)
{

	if (event == MG_NEW_REQUEST)
	{
		HttpEvent ev;
		ev.conn = conn;
		ev.req = request_info;
	
		//Login events
		if(strcmp(request_info->uri, "/l") == 0)
		{ do_login(ev);
		}
		else if(strcmp(request_info->uri, "/r") == 0)
		{ do_register(ev);
		}
		else if(strcmp(request_info->uri, "/q")	 == 0)
		{ do_logout(ev);
		}
		
		//Character creation events
		else if(strcmp(request_info->uri, "/t") == 0)
		{ do_list_players(ev);
		}
		else if(strcmp(request_info->uri, "/c") == 0)
		{ do_create_player(ev);
		}
		else if(strcmp(request_info->uri, "/j") == 0)
		{ do_join_game(ev);
		}
		else if(strcmp(request_info->uri, "/d") == 0)
		{ do_delete_player(ev);
		}
		
		//Game events
		else if(strcmp(request_info->uri, "/g") == 0)
		{ do_get_chunk(ev);
		}
		else if(strcmp(request_info->uri, "/h") == 0)
		{ do_heartbeat(ev);
		}
		
		//Unknown event type
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
void server_loop()
{
	char msg_buf[COMMAND_BUFFER_LEN];
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
			
			if(buf_ptr >= (COMMAND_BUFFER_LEN-1) || c == '\n')
			{
				msg_buf[buf_ptr] = '\0';
				handle_message(msg_buf, buf_ptr);
				buf_ptr = 0;
			}
			else
				msg_buf[buf_ptr++] = c;
		}
		
		usleep(1);
	}
}


//Server initialization
void init_app()
{
	init_login();
	init_sessions();
	
	game_instance = new World();
}


//Kills server
void shutdown_app()
{
	delete game_instance;
	shutdown_login();
}


//Program start point
int main(int argc, char** argv)
{
	init_app();
	
	//Start web server
	mg_context *context = mg_start(&event_handler, options);
	assert(context != NULL);
	cout << "Server started" << endl;
	
	//Run main thread
	server_loop();
	
	//Stop web server
	mg_stop(context);
	cout << "Server stopped" << endl;
	
	shutdown_app();

	return 0;
}

