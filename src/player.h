#ifndef MAILBOX_H
#define MAILBOX_H

#include <pthread.h>

#include <string>
#include <set>
#include <map>
#include <cstdint>

#include "constants.h"
#include "misc.h"
#include "chunk.h"
#include "entity.h"

#include "network.pb.h"

namespace Game
{
	//A player record
	struct Player
	{
		Player(Network::EntityState const& initial_state);
		~Player();
	
		//Player's entity id
		EntityID	entity_id;
		
		//Player name
		std::string name;
		
		//Client state information
		std::map<EntityID, Network::EntityState>	known_entities;
		std::set<VisID>								known_regions;
		
		//Pending network update packets
		Network::UpdatePacket	update_packet;
		Network::ChunkPacket	chunk_packet;
	};
	
	
	//The player data set
	struct PlayerSet
	{
		PlayerSet();
		~PlayerSet();
		
		//Player join/leave functions
		bool add_player(Network::EntityState const& initial_state);
		bool del_player(EntityID const& player_id);
		
		//Network functions, retrieves a packet for the given player
		bool get_update_packet(EntityID const& player_id, Network::UpdatePacket& packet);
		bool get_chunk_packet(EntityID const& player_id, Network::ChunkPacket& packet);
		
		//Callbacks for notifying players when an entity changes
		void on_entity_change(uint64_t tick, Network::Entity const& entity_state);
		void on_chunk_change(ChunkID const& chunk_id);
		
		//Chunk tracking stuff
		std::set<ChunkID> get_update_chunk_set(EntityID const& player_id);
		uint64_t get_next_vis_set(EntityID const& player_id);
		void mark_vis_set(EntityID const& player_id, uint64_t vis_id);
		
	private:
	
		//Lock for the player set
		pthread_mutex_t		player_lock;
		
		//List of players
		std::map<EntityID, Player*>		players;
	};

};


#endif

