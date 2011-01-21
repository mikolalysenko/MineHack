#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

//Hash stuff.  May swap this out for something different later (maybe tokyo cabinet hash?)
#include <ext/hash_map>
#include <ext/hash_map>

#include <cmath>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdlib>

#include "constants.h"
#include "misc.h"
#include "entity.h"
#include "mailbox.h"

using namespace std;
using namespace __gnu_cxx;
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
void Mailbox::add_player(EntityID const& player_id)
{
	PlayerLock	G(this, player_id);
	auto iter = player_data.find(player_id.id);
	if(iter != player_data.end())
		return;
	
	player_data[player_id.id] = new PlayerRecord();
}

//Removes a player
void Mailbox::del_player(EntityID const& player_id)
{
	PlayerLock	G(this, player_id);
	auto iter = player_data.find(player_id.id);
	if(iter == player_data.end())
		return;
		
	delete (*iter).second;
	player_data.erase(iter);
}

//---------------------------------------------------------------
// Packet header manipluation stuff
//---------------------------------------------------------------

//Sets the origin for a packet
void Mailbox::set_origin(EntityID const& player_id, int o_x, int o_y, int o_z)
{
	PlayerLock G(this, player_id);
	auto data = G.data();
	if(data == NULL)
		return;
	
	data->o_x = o_x;
	data->o_y = o_y;
	data->o_z = o_z;
}

//Sets the tick count for a packet
void Mailbox::set_tick(EntityID const& player_id, uint64_t tick_count)
{
	PlayerLock G(this, player_id);
	auto data = G.data();
	if(data == NULL)
		return;
		
	data->tick_count = tick_count;
}


//---------------------------------------------------------------
// Event posting
//---------------------------------------------------------------

//Updates a block on the client
void Mailbox::send_block(EntityID const& player_id, int x, int y, int z, Block b)
{
	PlayerLock G(this, player_id);
	auto data = G.data();
	if(data == NULL)
		return;

	data->block_events.push_back( BlockUpdate({ x, y, z, b }) );
}

//Logs a string to the client chat buffer
void Mailbox::send_chat(EntityID const& player_id, std::string const& msg, bool scrub_xml)
{
	PlayerLock G(this, player_id);
	auto data = G.data();
	if(data == NULL)
		return;

	//If no scrubbing is needed, then we just concatenate the message
	if(!scrub_xml)
	{
		data->chat_log.append(msg);
		return;
	}
	
	//Otherwise, as we do the concatenation we also scrub
	for(int i=0; i<msg.size(); i++)
	{
		char c = msg[i];
		
		switch(c)
		{
			case '<':
				data->chat_log.append("&lt;");
				continue;
				
			case '>':
				data->chat_log.append("&gt;");
				continue;
			
			case ' ':
				data->chat_log.append("&nbsp;");
				continue;

			case '&':
				data->chat_log.append("&amp;");
				continue;

			case '"':
				data->chat_log.append("&quot;");
				continue;
				
			default:
				data->chat_log.push_back(c);
		}
	}
	
	data->chat_log.append("<br/>");
}

//Send an entity update to the player
void Mailbox::send_entity(EntityID const& player_id, Entity const& entity)
{
	PlayerLock G(this, player_id);
	auto data = G.data();
	if(data == NULL)
		return;
	
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
// HTTP serialization
//---------------------------------------------------------------
void Mailbox::push_http_events(EntityID const& player_id, int socket)
{
	PlayerLock G(this, player_id);
	auto data = G.data();
	if(data == NULL)
		return;

	//Serialize data
	data->net_serialize(socket);
	
	//Clear out event buffer
	data->clear();
}

//---------------------------------------------------------------
// Packet methods
//---------------------------------------------------------------

//Network serialization structures
#pragma pack(push, 1)

//Network packet header
struct NetHeader
{
	uint64_t	tick_count;			//Packet tick count
	uint32_t	o_x, o_y, o_z;		//Packet origin
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
	uint16_t	x, y, z;			//Packed 16 bit network coordinate
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
		header.o_x = o_x;
		header.o_y = o_y;
		header.o_z = o_z;
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
			(int8_t)(b.x - o_x), 
			(int8_t)(b.y - o_y), 
			(int8_t)(b.z - o_z), 
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
		nc.x = (int16_t)((c.x - o_x) * COORD_NET_PRECISION);
		nc.y = (int16_t)((c.y - o_y) * COORD_NET_PRECISION);
		nc.z = (int16_t)((c.z - o_z) * COORD_NET_PRECISION);
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

template<typename T> void vcat(vector<uint8_t>& buf, T const& v)
{
	buf.insert(buf.end(), (uint8_t*)(void*)&v, (uint8_t*)(void*)(&v + 1));
}

//Serialize an entity
void Mailbox::PlayerRecord::serialize_entity(Entity const& ent)
{
	bool initialize = known_entities.count(ent.entity_id.id) == 0;
	if(initialize)
		known_entities.insert(ent.entity_id.id);

	//Entity packet format:
	//	Initialization flag
	//	EntityID
	//	( Initialization Data | Type specific data )

	eblob.push_back((uint8_t)initialize);
	vcat(eblob, ent.entity_id);
	
	//Store common data for coordinates (this gets written in a separate buffer, since it needs to be updated depending on what the entity state is)
	coords.push_back(Mailbox::Coordinate({
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



