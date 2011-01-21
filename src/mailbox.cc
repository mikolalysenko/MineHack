#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdlib>

#include "constants.h"
#include "misc.h"
#include "entity.h"
#include "mailbox.h"

using namespace std;
using namespace Game;


//---------------------------------------------------------------
//  Constructors
//---------------------------------------------------------------

//Mailbox constructor
Mailbox::Mailbox()
{
	pthread_mutex_init(&mailbox_lock, NULL);
}

//Destructor
Mailbox::~Mailbox()
{
	//Free up all old player records
	for(auto iter = player_data.begin(); iter != player_data.end(); ++iter)
	{
		delete (*iter).second;
	}

	//Release mutex
	pthread_mutex_destroy(&mailbox_lock);
}

//---------------------------------------------------------------
// Player management
//---------------------------------------------------------------

//Adds a player
void Mailbox::add_player(EntityID const& player_id, int ox, int oy, int oz)
{
	PlayerLock	G(this, player_id);
	auto iter = player_data.find(player_id.id);
	if(iter != player_data.end())
		return;

	auto data = new PlayerRecord();
	data->player_id = player_id;
	player_data[player_id.id] = data;
	set_index(data, ox, oy, oz);
}

//Removes a player
void Mailbox::del_player(EntityID const& player_id)
{
	PlayerLock	G(this, player_id);
	auto iter = player_data.find(player_id.id);
	if(iter == player_data.end())
		return;
		
	auto data = G.data();
	remove_index(data);
	player_data.erase(iter);
	delete data;
}

//---------------------------------------------------------------
// Packet header manipluation stuff
//---------------------------------------------------------------

//Sets the origin for a packet
void Mailbox::set_origin(EntityID const& player_id, int ox, int oy, int oz)
{
	PlayerLock G(this, player_id);
	auto data = G.data();
	if(data == NULL)
		return;
	
	update_index(data, ox, oy, oz);
}


//---------------------------------------------------------------
// Single event posting
//---------------------------------------------------------------

//Updates a block on the client
void Mailbox::send_block(EntityID const& player_id, int x, int y, int z, Block b)
{
	PlayerLock G(this, player_id);
	auto data = G.data();
	if(data == NULL)
		return;

	data->serialize_block( BlockUpdate({ x, y, z, b }) );
}

//Logs a string to the client chat buffer
void Mailbox::send_chat(EntityID const& player_id, std::string const& msg, bool scrub_xml)
{
	PlayerLock G(this, player_id);
	auto data = G.data();
	if(data == NULL)
		return;
		
	data->serialize_chat(msg, scrub_xml);
}

//Send an entity update to the player
void Mailbox::send_entity(EntityID const& player_id, Entity const& entity)
{
	PlayerLock G(this, player_id);
	auto data = G.data();
	if(data == NULL)
		return;
	
	data->tick_count = tick_count;
	data->serialize_entity(entity);
}

//Send a kill event
void Mailbox::send_kill(EntityID const& player_id, EntityID const& casualty)
{
	PlayerLock G(this, player_id);
	auto data = G.data();
	if(data == NULL)
		return;
	
	//Check that player knows about target entity
	auto iter = data->known_entities.find(casualty.id);
	if(iter == data->known_entities.end())
		return;
	
	data->known_entities.erase(iter);
	data->dead_entities.push_back(casualty);
}



//---------------------------------------------------------------
// Event broadcast / multicast
//---------------------------------------------------------------

//Broadcasts a block event
void Mailbox::broadcast_block(Region const& r, int x, int y, int z, Block b)
{
	struct Visitor
	{
		static void visit(PlayerRecord* data, void* block)
		{
			data->serialize_block(*(Mailbox::BlockUpdate*)block);	
		}
	};

	Mailbox::BlockUpdate block = { x, y, z, b };	
	foreach_region(r, Visitor::visit, &block);
}

//Broadcasts a chat event
void Mailbox::broadcast_chat(Region const& r, string const& msg, bool scrub_xml)
{
	struct Visitor
	{
		string msg;
		bool scrub;
		
		static void visit(PlayerRecord* data, void* visitor)
		{
			Visitor* V = (Visitor*)visitor;
			data->serialize_chat(V->msg, V->scrub);
		}
	};

	Visitor V = { msg, scrub_xml };
	foreach_region(r, Visitor::visit, &V);
}

//Broadcasts an entity update
void Mailbox::broadcast_entity(Region const& r, Entity const& entity)
{
	struct Visitor
	{
		Entity* entity;
		uint64_t tick_count;
	
		static void visit(PlayerRecord* data, void* user_data)
		{
			Visitor* v = (Visitor*)user_data;
			data->tick_count = v->tick_count;
			data->serialize_entity(*(v->entity));
		}
	};

	Visitor V = { const_cast<Entity*>(&entity), tick_count };
	foreach_region(r, Visitor::visit, &V);
}

//Broadcasts a kill update
void Mailbox::broadcast_kill(Region const& r, EntityID const& casualty)
{
	struct Visitor
	{
		static void visit(PlayerRecord* data, void* user_data)
		{
			data->serialize_kill(*(EntityID*)user_data);
		}
	};
	
	foreach_region(r, Visitor::visit, const_cast<EntityID*>(&casualty));
}


//---------------------------------------------------------------
// Entity replication management
//---------------------------------------------------------------

//Forces a player to forget about a particular entity
void Mailbox::forget_entity(EntityID const& player_id, EntityID const& forgotten)
{
	PlayerLock G(this, player_id);
	auto data = G.data();
	if(data == NULL)
		return;
	
	//Check that player knows about target entity
	auto iter = data->known_entities.find(forgotten.id);
	if(iter == data->known_entities.end())
		return;
	
	data->known_entities.erase(iter);
}


//Forces a player to forget about all entities
void Mailbox::forget_all(EntityID const& player_id)
{
	PlayerLock G(this, player_id);
	auto data = G.data();
	if(data == NULL)
		return;
		
	data->known_entities.clear();
}



//---------------------------------------------------------------
// Index maintenence
//---------------------------------------------------------------

//Initializes the index for a player
void Mailbox::set_index(PlayerRecord* data, int ox, int oy, int oz)
{
	uint64_t bucket = TO_BUCKET(ox, oy, oz);
	
	data->ox = ox;
	data->oy = oy;
	data->oz = oz;
	
	auto iter = player_pos_index.find(bucket);
	if(iter == player_pos_index.end())
	{
		player_pos_index[bucket] = vector<EntityID>( { data->player_id } );
	}
	else
	{
		(*iter).second.push_back(data->player_id);
	}
}

//Updates the index for a player
void Mailbox::update_index(PlayerRecord* data, int ox, int oy, int oz)
{
	int nbucket = TO_BUCKET(ox, oy, oz),
		pbucket = TO_BUCKET(data->ox, data->oy, data->oz);
		
	if(pbucket == nbucket)
		return;

	remove_index(data);
	set_index(data, ox, oy, oz);
}

//Removes a player from the index
void Mailbox::remove_index(PlayerRecord* data)
{
	uint64_t bucket = TO_BUCKET(data->ox, data->oy, data->oz);

	auto iter = player_pos_index.find(bucket);
	if(iter == player_pos_index.end())
	{
		cout << "This should never happen" << endl;
		return;
	}

	vector<EntityID>& v = (*iter).second;
	int s = v.size();
	for(int i=0; i<s; i++)
	{
		if(v[i] == data->player_id)
		{
			v[i] = v[s-1];
			v.pop_back();
			break;
		}
	}
	
	if(v.size() == 0)
	{
		player_pos_index.erase(iter);
	}
}

//Visits each player in a range
void Mailbox::foreach_region(
	Region const& r,
	void (*func)(PlayerRecord*, void*), 
	void* user_data)
{
	//Lock database
	MutexLock L(&mailbox_lock);
	
	//If region is entire map, then we post event to all players
	if(r.lo <= 0)
	{
		for(auto iter=player_data.begin(); iter!=player_data.end(); ++iter)
			(*func)(iter->second, user_data);
		return;	
	}
	
	int bmin_x = (int)(r.lo[0] / BUCKET_X),
		bmin_y = (int)(r.lo[1] / BUCKET_Y),
		bmin_z = (int)(r.lo[2] / BUCKET_Z),
		bmax_x = (int)(r.hi[0] / BUCKET_X),
		bmax_y = (int)(r.hi[1] / BUCKET_Y),
		bmax_z = (int)(r.hi[2] / BUCKET_Z);
	
	for(int bx=bmin_x; bx<=bmax_x; bx++)
	for(int by=bmin_y; by<=bmax_y; by++)
	for(int bz=bmin_z; bz<=bmax_z; bz++)
	{
		uint64_t bucket = BUCKET_IDX(bx, by, bz);
		
		auto iter=player_pos_index.find(bucket);
		if(iter == player_pos_index.end())
			continue;
			
		int s = iter->second.size();
		for(int i=0; i<s; i++)
		{
			EntityID player_id = iter->second[i];
			(*func)(player_data[player_id.id], user_data);
		}
	}
}

//---------------------------------------------------------------
// Network Packet Construction Methods
//---------------------------------------------------------------

//Network serialization structures
#pragma pack(push, 1)

//Network packet header
struct NetHeader
{
	uint64_t	tick_count;			//Packet tick count
	uint32_t	ox, oy, oz;			//Packet origin
	uint16_t	block_event_size;	//Number of block events
	uint16_t	chat_log_size;		//Size of the chat log
	uint16_t	coord_size;			//Number of entity coords
	uint16_t	update_size;		//Number of entity updates
	uint16_t	kill_size;			//Number of dead entities
};

//Network block event
struct NetBlock
{
	int8_t	x, y, z;				//Block event position
	Block b;						//Block type
};

//A network coordinate
struct NetCoord
{
	uint16_t	t;					//Time coordinate (relative to start time)
	int16_t		x, y, z;			//Packed 16 bit network coordinate
	uint8_t 	pitch, yaw, roll;	//8-bit encoded orientation
};
#pragma pack(pop)



//Length of the network packet
int Mailbox::PlayerRecord::packet_len() const
{
	return	sizeof(NetHeader) +
			sizeof(NetBlock) * block_events.size() +
			chat_log.size() +
			sizeof(NetCoord) * coords.size() +
			eblob.size() +
			sizeof(EntityID) * dead_entities.size();
}

//Serialize the packet into binary form on the socket
void Mailbox::PlayerRecord::net_serialize(int socket) const
{
	//Send HTTP header
	{	char header_buf[1024];
		int len = snprintf(header_buf, sizeof(header_buf),
			"HTTP/1.1 200 OK\n"
			"Cache: no-cache\n"
			"Content-Type: application/octet-stream; charset=x-user-defined\n"
			"Content-Transfer-Encoding: binary\n"
			"Content-Length: %d\n"
			"\n", packet_len());
	
		send(socket, header_buf, len, 0);
	}

	//Send header
	{	NetHeader header;
		header.tick_count = tick_count;
		header.ox = ox;
		header.oy = oy;
		header.oz = oz;
		header.block_event_size = block_events.size();
		header.chat_log_size = chat_log.size();
		header.coord_size = coords.size();
		header.update_size = eblob.size();
		header.kill_size = dead_entities.size();	
		
		assert(sizeof(NetHeader) == 30);
		
		send(socket, &header, sizeof(NetHeader), 0);
	}
	
	//Send block events
	for(auto iter = block_events.begin(); iter != block_events.end(); ++iter)
	{
		Mailbox::BlockUpdate b = *iter;
		NetBlock nb = { 
			(int8_t)(b.x - ox), 
			(int8_t)(b.y - oy), 
			(int8_t)(b.z - oz), 
			b.b };
		send(socket, &nb, sizeof(NetBlock), 0);
	}
	
	//Send chat log
	send(socket, chat_log.data(), chat_log.size(), 0);
	
	//Send entity coordinates
	for(auto iter = coords.begin(); iter != coords.end(); ++iter)
	{
		Coordinate const& c = *iter;
		NetCoord nc;
		nc.t = (uint16_t)(tick_count - c.t);
		nc.x = (int16_t)((c.x - ox) * COORD_NET_PRECISION);
		nc.y = (int16_t)((c.y - oy) * COORD_NET_PRECISION);
		nc.z = (int16_t)((c.z - oz) * COORD_NET_PRECISION);
		nc.pitch = (uint8_t)(((int)(c.pitch * 255.0 / (2.0 * M_PI)) + 0x1000) & 0xff);
		nc.yaw	 = (uint8_t)(((int)(c.yaw * 255.0 / (2.0 * M_PI)) + 0x1000) & 0xff);
		nc.roll	 = (uint8_t)(((int)(c.roll * 255.0 / (2.0 * M_PI)) + 0x1000) & 0xff);
		send(socket, &nc, sizeof(NetCoord), 0);
	}
	
	//Send entity blob
	send(socket, &eblob[0], eblob.size(), 0);
	
	//Send dead entities
	send(socket, &dead_entities[0], dead_entities.size() * sizeof(EntityID), 0);
}

//Resets the packet
void Mailbox::PlayerRecord::clear()
{
	block_events.clear();
	chat_log.clear();
	coords.clear();
	eblob.clear();
	dead_entities.clear();
}


//---------------------------------------------------------------
//	Entity serialization
//---------------------------------------------------------------


//Serialize a block event
void Mailbox::PlayerRecord::serialize_block(Mailbox::BlockUpdate const& b)
{
	block_events.push_back(b);
}

//Serialize a chat message
void Mailbox::PlayerRecord::serialize_chat(string const& msg, bool scrub_xml)
{
	//If no scrubbing is needed, then we just concatenate the message
	if(!scrub_xml)
	{
		chat_log.append(msg);
		return;
	}
	
	//Otherwise, as we do the concatenation we also scrub
	for(int i=0; i<msg.size(); i++)
	{
		char c = msg[i];
		
		switch(c)
		{
			case '<':
				chat_log.append("&lt;");
				continue;
				
			case '>':
				chat_log.append("&gt;");
				continue;
			
			case ' ':
				chat_log.append("&nbsp;");
				continue;

			case '&':
				chat_log.append("&amp;");
				continue;

			case '"':
				chat_log.append("&quot;");
				continue;
				
			default:
				chat_log.push_back(c);
		}
	}
	chat_log.append("<br/>");
}

//Serialize an entity destruction event
void Mailbox::PlayerRecord::serialize_kill(EntityID const& casualty)
{
	dead_entities.push_back(casualty);
}

//Helper function for entity serialization
template<typename T> void vcat(vector<uint8_t>& buf, T const& v)
{
	buf.insert(buf.end(), (uint8_t*)(void*)&v, (uint8_t*)(void*)(&v + 1));
}

//Serialize an entity
void Mailbox::PlayerRecord::serialize_entity(Entity const& ent)
{
	bool initialize = known_entities.count(ent.entity_id.id) == 0;
	if(initialize && (ent.base.flags & EntityFlags::Persist))
		known_entities.insert(ent.entity_id.id);

	//Entity packet format:
	//	Initialization flag
	//	EntityID
	//	( Initialization Data | Type specific data )

	eblob.push_back((uint8_t)initialize);
	vcat(eblob, ent.entity_id);
	
	//Store common data for coordinates (this gets written in a separate buffer, since it needs to be updated depending on what the entity state is)
	coords.push_back(Mailbox::Coordinate({
		tick_count,
		ent.base.x, ent.base.y, ent.base.z,
		ent.base.pitch, ent.base.yaw, ent.base.roll }));
	
	if(initialize)
	{
		//Initialization data:
		//	Type
		//	( Type initialization )
		vcat(eblob, ent.base.type);
		
		switch(ent.base.type)
		{
			case EntityType::Player:
				initialize_player(ent.player);
			break;
			
			case EntityType::Monster:
				initialize_monster(ent.monster);
			break;
		}
	}
	else
	{
		//Update data (assumes client already has a copy of this entity):
		//	(Type specific update data)
		serialize_player(ent.player);
		
		switch(ent.base.type)
		{
			case EntityType::Player:
				serialize_player(ent.player);
			break;
			
			case EntityType::Monster:
				serialize_monster(ent.monster);
			break;
		}
	}
}

//Network initialize a player
void Mailbox::PlayerRecord::initialize_player(PlayerEntity const& ent)
{
	//Send player name to client
	int name_len = strlen(ent.player_name);
	eblob.push_back((uint8_t)name_len);
	eblob.insert(eblob.end(), ent.player_name, ent.player_name + name_len);
}

//Serialize a player
void Mailbox::PlayerRecord::serialize_player(PlayerEntity const& ent)
{
}

//Send monster initialization packet
void Mailbox::PlayerRecord::initialize_monster(MonsterEntity const& ent)
{
}

//Serialize a monster
void Mailbox::PlayerRecord::serialize_monster(MonsterEntity const& ent)
{
}



