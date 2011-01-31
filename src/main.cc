#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

#include <iostream>
#include <sstream>
#include <cassert>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <algorithm>

#include <tcutil.h>

#include "mongoose.h"

#include "constants.h"
#include "login.h"
#include "session.h"
#include "misc.h"
#include "world.h"
#include "action.h"

using namespace std;
using namespace Server;
using namespace Game;


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

//The HttpBlob reader just pulls all the text out of a body in binary form
struct HttpBlobReader
{
	int len;
	uint8_t* data;
	
	HttpBlobReader(mg_connection* conn) : len(-1), data(NULL)
	{
		//Read number of available bytes
		int avail = mg_available_bytes(conn);
		if(avail < 0)
		{	avail = -1;
			return;
		}
		
		//Allocate buffer
		data = (uint8_t*)malloc(avail);
		len = mg_read(conn, (void*)data, avail);
		
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
	const char* AJAX_HEADER = 	  
	  "HTTP/1.1 200 OK\n"
	  "Cache: no-cache\n"
	  "Content-Type: application/x-javascript\n"
	  "\n";

	char buf[1024];

	va_list args;
	va_start(args, fmt);	
	int len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	
	if(len < 0)
	{
		ajax_error(conn);
		return;
	}

	//Push data to client
	mg_write(conn, AJAX_HEADER, strlen(AJAX_HEADER));
	mg_write(conn, buf, len);
}

//Sends a binary blob to the client
bool ajax_send_binary(mg_connection *conn, const void* buf, size_t len)
{
	int sz;
	ScopeFree G((void*)tcdeflate((const char*)buf, len, &sz));
	
	cout << "Old size = " << len << ", Compressed size = " << sz << endl;

	mg_printf(conn,
	  "HTTP/1.1 200 OK\n"
	  "Cache: no-cache\n"
	  "Content-Type: application/octet-stream; charset=x-user-defined\n"
	  "Content-Transfer-Encoding: binary\n"
	  "Content-Encoding: deflate\n"
	  "Content-Length: %d\n"
	  "\n", sz);

	return send(mg_steal_socket(conn), G.ptr, sz, 0) >= 0;
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
	char session_id_str[24];	
	memset(session_id_str, 0, sizeof(session_id_str));
	
	const char* qs = request_info->query_string;
	size_t qs_len = (qs == NULL ? 0 : strlen(qs));
	
	int len = mg_get_var(qs, qs_len, "k", session_id_str, sizeof(session_id_str));
	
	if(len < 0)
		return false;
	
	//Scan in the session id
	char* ptr;
	session_id.id = strtoull(session_id_str, &ptr, 16);
	
	if(valid_session_id(session_id))
		return true;
	
	return false;
}

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
			"Missing username/password");
	}
	else if(create_account(username, password_hash))
	{
		do_login(ev);
	}
	else
	{
		ajax_printf(ev.conn,
			"Fail\n"
			"Username already in use");
	}
}


//Handle a user login
void do_login(HttpEvent& ev)
{
	string user_name, password_hash;
	
	if(	!get_string(ev.req, "n", user_name) ||
		!get_string(ev.req, "p", password_hash) )
	{
		ajax_printf(ev.conn, 
			"Fail\n"	
			"Missing username/password");
		
		return;
	}

	SessionID session_id;
	if( verify_user_name(user_name, password_hash) &&
		create_session(user_name, session_id) )
	{
		cout << "session id = " << session_id.id << endl;
	
		ajax_printf(ev.conn, 
			"Ok\n"
			"%016llx", (unsigned long long int)session_id.id);
	}
	else
	{
		ajax_printf(ev.conn,
			"Fail\n"
			"Could not log in");
	}
}


//Handle user log out
void do_logout(HttpEvent& ev)
{
	cout << "Logout" << endl;

	SessionID session_id;
	if(get_session_id(ev.req, session_id))
	{
		Session session;
		if(get_session_data(session_id, session) &&
			session.state == SessionState::InGame )
		{
			game_instance->player_leave(session.player_id);
		}
	
		delete_session(session_id);
	}

	ajax_printf(ev.conn,		
		"Ok\n"
		"Logout successful");
}



// --------------------------------------------------------------------------------
//    Character select event handlers
// --------------------------------------------------------------------------------

void do_create_player(HttpEvent& ev)
{
	cout << "Creating player" << endl;

	//Grab request variables
	SessionID	session_id;
	string		player_name;
	Session		session;
	
	if( !get_session_id(ev.req, session_id) ||
		!get_string(ev.req, "player_name", player_name) ||
		!get_session_data(session_id, session) ||
		session.state != SessionState::PreJoinGame )
	{
		ajax_error(ev.conn);
		return;
	}
	
	EntityID player_id;
	if( game_instance->player_create(player_name, player_id) &&
		add_player_name(session.user_name, player_name) )
	{
		ajax_printf(ev.conn, 
			"Ok\n"
			"Created player successfully");
	}
	else
	{
		//Roll back changes
		game_instance->player_delete(player_id);
		remove_player_name(session.user_name, player_name);
	
		ajax_printf(ev.conn,
			"Fail\n"
			"Player name is already in use");
	}
}

void do_join_game(HttpEvent& ev)
{
	cout << "Joining game" << endl;

	//Grab request variables
	SessionID		session_id;
	string			player_name;
	Session			session;
	vector<string>	player_names;
	
	if( !get_session_id(ev.req, session_id) ||
		!get_string(ev.req, "player_name", player_name) ||
		!get_session_data(session_id, session) ||
		session.state != SessionState::PreJoinGame ||
		!get_player_names(session.user_name, player_names) ||
		find(player_names.begin(), player_names.end(), player_name) == player_names.end() )
	{
		ajax_error(ev.conn);
		return;
	}


	EntityID player_id;
	if( game_instance->get_player_entity(player_name, player_id) &&
		game_instance->player_join(player_id) &&	
		set_session_player(session_id, player_id) )
	{
		ajax_printf(ev.conn,
			"Ok\n"
			"Successfully joined game\n"
			"%016llx\n", player_id.id);
	}
	else
	{
		game_instance->player_leave(player_id);
		ajax_error(ev.conn);
	}

}

void do_list_players(HttpEvent& ev)
{
	//Grab request variables
	SessionID		session_id;
	Session			session;
	vector<string>	player_names;
	
	if( !get_session_id(ev.req, session_id) ||
		!get_session_data(session_id, session) ||
		session.state != SessionState::PreJoinGame ||
		!get_player_names(session.user_name, player_names) )
	{
		ajax_error(ev.conn);
		return;
	}

	ajax_printf(ev.conn, "Ok\n");
	for(int i=0; i<player_names.size(); i++)
	{
		mg_printf(ev.conn, "%s\n", player_names[i].c_str());
	}
}

void do_delete_player(HttpEvent& ev)
{
	//Grab request variables
	SessionID	session_id;
	string		player_name;
	Session		session;
	EntityID	player_id;
	
	if( !get_session_id(ev.req, session_id) ||
		!get_string(ev.req, "player_name", player_name) ||
		!get_session_data(session_id, session) ||
		session.state != SessionState::PreJoinGame ||
		!game_instance->get_player_entity(player_name, player_id) ||
		!remove_player_name(session.user_name, player_name) ) 
	{
		ajax_error(ev.conn);
		return;
	}
	
	game_instance->player_delete(player_id);
	
	ajax_printf(ev.conn,
		"Ok\n"
		"Successfully removed player");
}



// --------------------------------------------------------------------------------
//    Game event handlers
// --------------------------------------------------------------------------------

#pragma pack(push,1)

struct NetChunk
{
	int8_t	dx, dy, dz;
};

#pragma pack(pop)

//Retrieves a chunk
void do_get_chunk(HttpEvent& ev)
{
	HttpBlobReader blob(ev.conn);
	
	//Read out the session id
	if( blob.len <= sizeof(SessionID) + sizeof(ChunkID) ||
		((blob.len - sizeof(SessionID)) % 3 != 0) )
	{
		cout << "Get chunk request is invalid size!" << endl;
		ajax_error(ev.conn);
		return;
	}
	
	SessionID	session_id = *((SessionID*)blob.data);
	Session		session;
	
	if(!get_session_data(session_id, session) || 
		session.state != SessionState::InGame)
	{
		cout << "Player is not logged in!" << endl;
		ajax_error(ev.conn);
		return;
	}
	
	//Allocate chunk buffer
	int n_chunks		= (blob.len - sizeof(SessionID) - sizeof(ChunkID)) / sizeof(NetChunk);
	int max_buf_len		= n_chunks * sizeof(Chunk);
	
	
	ScopeFree G(malloc(max_buf_len));
	assert(G.ptr != NULL);
	
	uint8_t	*buf_ptr 	= (uint8_t*)G.ptr;
	int		buf_len 	= 0;
	
	//Add a leading 1 to avoid making a weird byte order mask and confusing the client
	*(buf_ptr++) = 1;
	buf_len++;
	
	//Get all pending chunks
	ChunkID base_chunk	= *(ChunkID*)(blob.data + sizeof(SessionID));
	NetChunk* chunk_end = (NetChunk*)(blob.data + blob.len);
	
	for(NetChunk* chunk_ptr = (NetChunk*)(blob.data + sizeof(SessionID) + sizeof(ChunkID)); 
		chunk_ptr < chunk_end; 
		++chunk_ptr)
	{
		//Compute chunk id
		ChunkID chunk_id(
			base_chunk.x + chunk_ptr->dx, 
			base_chunk.y + chunk_ptr->dy, 
		  	base_chunk.z + chunk_ptr->dz);
	
		//Extract the chunk
		int len = game_instance->get_compressed_chunk(
			session.player_id, chunk_id, buf_ptr, max_buf_len - buf_len);
	
		if(len < 0)
		{
			cout << "Ran out of space in chunk buffer" << endl;
			break;
		}
			
		buf_ptr += len;
		buf_len += len;
	}
	
	if(!ajax_send_binary(ev.conn, G.ptr, buf_len))
	{
		cout << "Send failed, killing connection" << endl;
		game_instance->player_leave(session.player_id);
		delete_session(session_id);
	}
}

//Network packet header
#pragma pack(push,1)

struct NetInputHeader
{
	SessionID	session_id;
	
	uint64_t	tick;
	uint32_t	ix, iy, iz;
	uint8_t		ipitch, iyaw, iroll;
	uint8_t		key_state;
	
	uint16_t	forget_size;
	uint16_t	chat_size;
	uint16_t	action_size;
};

struct NetActionHeader
{
	ActionType	type;
	uint8_t		target_type;
	uint16_t	delta_tick;
};

struct NetBlockTarget
{
	int8_t		x, y, z;
};

struct NetRayTarget
{
	int16_t	ox, oy, oz,
			dx, dy, dz;
};

#pragma pack(pop)

//Pulls pending events from client
void do_heartbeat(HttpEvent& ev)
{
	HttpBlobReader blob(ev.conn);
	
	if(blob.len < sizeof(NetInputHeader))
	{
		cout << "Blob is missing header, header len = " << sizeof(NetInputHeader) << ", packet len = " << blob.len << endl;
		ajax_error(ev.conn);
		return;
	}
	
	NetInputHeader header = *(NetInputHeader*)(void*)blob.data;
	Session		session;
	
	int packet_len = sizeof(NetInputHeader) +
					 sizeof(EntityID) * header.forget_size +
					 header.chat_size +
					 header.action_size;
	
	/*				 
	cout << "Expect: " << packet_len << ", got: " << blob.len << endl;
	
	cout << "Packet header: " << endl
		<< "tick = " << header.tick << endl
		<< "forget size = " << header.forget_size << endl
		<< "chat size = " << header.chat_size << endl;
	*/
	
	//Check session is valid
	if( !get_session_data(header.session_id, session) ||
		session.state != SessionState::InGame || 
		blob.len != packet_len )
	{
		cout << "Not logged in yet" << endl;
		ajax_error(ev.conn);
		return;
	}
	
	
	//Check player is valid
	if(	!game_instance->valid_player(session.player_id) )
	{
		cout << "Player is invalid!" << endl;
	
		delete_session(header.session_id);
		ajax_error(ev.conn);
		return;
	}
	
	//Parse out player input
	PlayerEvent pl;
	uint64_t last_tick;
	pl.tick		= header.tick;
	pl.x		= ((double)header.ix) / COORD_NET_PRECISION;
	pl.y		= ((double)header.iy) / COORD_NET_PRECISION;
	pl.z		= ((double)header.iz) / COORD_NET_PRECISION;
	pl.pitch	= ((double)header.ipitch)	/ 255.0 * 2.0 * M_PI;
	pl.yaw		= ((double)header.iyaw)		/ 255.0 * 2.0 * M_PI;
	pl.roll		= ((double)header.iroll)	/ 255.0 * 2.0 * M_PI;
	pl.input_state = header.key_state;
	
	//Sanity check packet
	if(header.tick > game_instance->tick_count)
	{
		ajax_error(ev.conn);
		return;
	}
	
	game_instance->handle_player_tick(session.player_id, pl, last_tick);
	
	//cout << "heartbeat:  last = " << last_tick << ", cur = " << header.tick << endl;
	
	//Parse out the events
	uint8_t *ptr = blob.data + sizeof(NetInputHeader),
			*eob = blob.data + blob.len;

	//Handle forget events
	for(int i=0; i<header.forget_size; i++)
	{
		EntityID entity_id = *(EntityID*)(void*)ptr;
		ptr += sizeof(EntityID);
		game_instance->handle_forget(session.player_id, entity_id);
	}

	//Handle chat message	
	if(header.chat_size > 0)
	{
		game_instance->handle_chat(session.player_id, string((const char*)ptr, header.chat_size));
		ptr += header.chat_size;
	}
	
	//Handle action events
	while(ptr < eob)
	{
		//Read out action header
		if(ptr + sizeof(NetActionHeader) > eob)
			break;
		NetActionHeader action_header = *(NetActionHeader*)(void*)ptr;
		ptr += sizeof(NetActionHeader);
		
		
		//Calculate action size
		int target_size = 0;
		switch(action_header.target_type)
		{
			case ActionTarget::Block:
				target_size += sizeof(NetBlockTarget);
			break;
			
			case ActionTarget::Entity:
				target_size += sizeof(EntityID);
			break;
			
			case ActionTarget::Ray:
				target_size += sizeof(NetRayTarget);
			break;
		
			default:
				break;
		}
		
		//Check bounds
		if(eob < ptr + target_size)
		{
			cout << "Invalid target size for action event?" << endl;
			break;
		}
		
		Action action;
		action.type			= action_header.type;
		action.target_type	= action_header.target_type;
		action.tick			= header.tick - action_header.delta_tick;
		
		cout << "Got action: " << (int)action.type << ',' << action.target_type << " @ " << action.tick << endl;

		
		if(action.target_type == ActionTarget::Block)
		{
			NetBlockTarget target = *(NetBlockTarget*)(void*)ptr;
			ptr += sizeof(NetBlockTarget);
			
			action.target_block.x = target.x + (int)pl.x;
			action.target_block.y = target.y + (int)pl.y;
			action.target_block.z = target.z + (int)pl.z;
			
			cout << "base = " << (int)pl.x << ',' << (int)pl.y << ',' << (int)pl.z << endl;
			cout << "offset = " << (int)target.x << ',' << (int)target.y << ',' << (int)target.z << endl;
			cout << "Target block = " << action.target_block.x << ',' << action.target_block.y << ',' << action.target_block.z << endl;
		}
		else if(action.target_type == ActionTarget::Entity)
		{
			action.target_id = *(EntityID*)(void*)ptr;
			ptr += sizeof(EntityID);
		}
		else if(action.target_type == ActionTarget::Ray)
		{
			NetRayTarget target = *(NetRayTarget*)(void*)ptr;
			ptr += sizeof(NetRayTarget);
			
			action.target_ray.ox = ((double)target.ox)/COORD_NET_PRECISION + pl.x;
			action.target_ray.oy = ((double)target.oy)/COORD_NET_PRECISION + pl.y;
			action.target_ray.oz = ((double)target.oz)/COORD_NET_PRECISION + pl.z;
			
			double l = sqrt(target.dx*target.dx + target.dy*target.dy + target.dz*target.dz);
			
			action.target_ray.dx = (double)target.dx / l;
			action.target_ray.dy = (double)target.dy / l;
			action.target_ray.dz = (double)target.dz / l;
		}
		
		//Quick sanity check on action, should not have happened *before*
		//last player update packet.  Discard all out of order events.
		if(action.tick < last_tick)
		{
			cout << "Action is out of order" << endl;
			continue;
		}
		last_tick = action.tick;
		
		game_instance->handle_action(session.player_id, action);
		
	}
	
	//Write data directly to socket (nasty, but saves having to allocate extra buffers)
	//cout << "Serializing heartbeat packet" << endl;
	if(!game_instance->heartbeat(session.player_id, mg_steal_socket(ev.conn)))
	{
		cout << "Heartbeat failed" << endl;
		delete_session(header.session_id);
		ajax_error(ev.conn);
	}
}


// --------------------------------------------------------------------------------
//    Server interface code
// --------------------------------------------------------------------------------

//The event dispatcher
static void *event_handler(mg_event event,
                           mg_connection *conn,
                           const mg_request_info *request_info)
{
	if (event == MG_NEW_REQUEST && 
		request_info->uri[0] == '/' &&
		request_info->uri[1] != '\0' &&
		request_info->uri[2] == '\0' )
	{
		HttpEvent ev;
		ev.conn = conn;
		ev.req = request_info;

		//Events are determined by a single character
		switch(request_info->uri[1])
		{
			//Game events
			case 'h':
				do_heartbeat(ev);
			break;
			
			case 'g':
				do_get_chunk(ev);
			break;
		
		
			//Login events
			case 'l':
				do_login(ev);
			break;
			
			case 'r':
				do_register(ev);
			break;
			
			case 'q':
				do_logout(ev);
			break;
			
			//Character creation events
			case 't':
				do_list_players(ev);
			break;
			
			case 'c':
				do_create_player(ev);
			break;
			
			case 'j':
				do_join_game(ev);
			break;
			
			case 'd':
				do_delete_player(ev);
			break;
			
			//Unknown event type
			default:
				return NULL;
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
		
		usleep(SLEEP_TIME * 1000);
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
	assert(sizeof(long long int) == sizeof(int64_t));
	
	srand(time(NULL));

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

