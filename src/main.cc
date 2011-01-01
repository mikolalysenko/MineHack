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

//Server initialization
void init()
{
	init_login();
	init_sessions();
}

//Retrieves the session variable associated to a given http request
Session* get_session(mg_connection * conn)
{
	char session_str[64];
	mg_get_cookie(conn, "S", session_str, sizeof(session_str));
	
	stringstream ss(session_str);
	SessionID key;
	ss >> key;
	
	return lookup_session(key);
}

//Create an account
void do_register(
	mg_connection* conn, 
	const mg_request_info* request_info)
{
}

//Handle a user login
void do_login(
	mg_connection* conn, 
	const mg_request_info* request_info)
{
}

//Handle user log out
void do_logout(
	Session* session,
	mg_connection* conn, 
	const mg_request_info* request_info)
{
}


//Event dispatch
static void *event_handler(enum mg_event event,
                           struct mg_connection *conn,
                           const struct mg_request_info *request_info)
{
	const char *processed = "yes";

	if (event == MG_NEW_REQUEST)
	{
		cout << "Got event: " << request_info->uri << endl;
	
		//Retrieve session cookie
		Session* session = get_session(conn);
		
		//Not logged in
		if(session == NULL)
		{
			if(strcmp(request_info->uri, "/l") == 0)
			{ do_login(conn, request_info);
			}
			else if(strcmp(request_info->uri, "/r") == 0)
			{ do_register(conn, request_info);
			}
			else
			{ processed = NULL;
			}
		}
		//User logged in
		else
		{
			if(strcmp(request_info->uri, "/q")	 == 0)
			{ do_logout(session, conn, request_info);
			}
			else
			{ processed = NULL;
			}
		}
	}

	return (void*)processed;
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

