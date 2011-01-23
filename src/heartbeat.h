#ifndef HEARTBEAT_H
#define HEARTBEAT_H

namespace Game
{
	//Needed to resolve circular reference
	class Mailbox;
	class EntityDB;
	bool heartbeat_impl(Mailbox* mailbox, EntityDB* entity_db, EntityID const& player_id, int tick_count, int socket);
};

#endif
