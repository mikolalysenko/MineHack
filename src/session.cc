#include <iostream>

#include "session.h"

using namespace std;

namespace Sessions
{


std::ostream& operator<<(std::ostream& os, const SessionID& id)
{
	return os;
}

std::istream& operator>>(std::istream& is, SessionID& id)
{
	return is;
}


void init_sessions()
{
}

Session* create_session()
{
	return NULL;
}

Session* lookup_session(SessionID id)
{
	return NULL;
}

void delete_session(SessionID id)
{
}




};
