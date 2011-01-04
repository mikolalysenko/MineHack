#include <iostream>
#include <sstream>
#include <cassert>
#include <cstring>

#include "mongoose.h"
#include "login.h"
#include "session.h"

using namespace std;
using namespace Login;
using namespace Sessions;

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

//Server initialization
void init()
{
	init_login();
	init_sessions();
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
		else
		{ return NULL;
		}
		
		return (void*)"yes";
	}
	
	return NULL;
}


int main(int argc, char** argv)
{
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

