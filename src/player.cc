#include <pthread.h>

#include <string>
#include <set>
#include <map>
#include <cstdint>

#include <google/protobuf/io/gzip_stream.h>

#include "constants.h"
#include "misc.h"
#include "chunk.h"
#include "entity.h"
#include "player.h"

#include "network.pb.h"

using namespace std;
using namespace Game;

Player::Player(const Network::EntityState& initial_state)
{
	entity_id = initial_state.entity_id();
	name = initial_state.player_state.player_name();
	
	known_entities[entity_id] = initial_state;
}

Player::~Player()
{
}

//Initialize player set
PlayerSet::PlayerSet()
{
	//Initialize player lock
	pthread_mutex_init(&player_lock, NULL);
}

//Player set deletion
PlayerSet::~PlayerSet()
{
}

//Add a player to the set
bool PlayerSet::add_player(const Network::EntityState& initial_state)
{
	//Lock database
	MutexLock L(&player_lock);
	
	//Check if player already exists
	uint64_t player_id = intitial_state.entity_id();

	//Verify that player does not exist
	auto iter = players.find(player_id);
	if(iter != players.end())
		return false;
	
	players[player_id] = new Player(intial_state);
	return true;
}

//Remove a player from the set
bool PlayerSet::del_player(EntityID const& player_id)
{
	//Lock database
	MutexLock L(&player_lock);
	
	auto iter = players.find(player_id);
	if(iter == players.end())
		return false;
		
	players.erase(iter);
	return true;
}

//Retrieves the pending network packet for a player
bool PlayerSet::get_network_packet(EntityID const& player_id, Network::UpdatePacket& packet)
{
	MutexLock L(&player_lock);

	auto iter = players.find(player_id);
	if(iter == players.end())
		return false;
		
	packet.Swap((*iter).second->update_packet);
	return true;
}


//Retrieves pending chunk packet for a player
bool PlayerSet::get_chunk_packet(EntityID const& player_id, Network::ChunkPacket& packet)
{
	MutexLock L(&player_lock);

	auto iter = players.find(player_id);
	if(iter == players.end())
		return false;
		
	packet.Swap((*iter).second->chunk_packet);
	return true;
}


//Callbacks for notifying players when an entity changes
void on_entity_change(Network::EntityState const& entity_state)
{
	//Only update entities with a position on the client
	if(!entity_state.has_position())
		return;
	
	uint64_t entity_id = entity_state.entity_id();
	auto vis_id = VisID(entity_state.position());

	MutexLock L(&player_lock);
	
	//Dumb algorithm: Look through all players
	//FIXME: Implement indexing based on visible regions
	for(auto iter = players.begin(); iter!=players.end(); ++iter)
	{
		Player* p = (*iter).second;
		if(p->known_regions.find(vis_id.hash()) == p->known_regions.end())
			continue;
		
		//FIXME: Need to diff entity against previously known state
	}

}

//Called when a chunk has changed
void on_chunk_change(Network::Chunk const& delta_chunk)
{
	
}

//Chunk tracking stuff
std::set<ChunkID> PlayerSet::get_update_chunk_set(EntityID const& player_id)
{
}

uint64_t PlayerSet::get_next_vis_set(EntityID const& player_id)
{
}

void PlayerSet::mark_vis_set(EntityID const& player_id, uint64_t vis_id)
{
}
