#ifndef ENTITY_H
#define ENTITY_H

#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>

#include "constants.h"
#include "config.h"
#include "chunk.h"

//Protocol buffer for entity
#include "network.pb.h"

namespace Game
{
	//An entity ID
	typedef std::uint64_t EntityID;

	//The entity state type
	struct Entity
	{
		int entity_flags;
	
		//The state buffers
		Network::EntityState state[2];
	};

	//Manages a collection of entities
	struct EntityDB
	{
		//Constructor/destructor stuff
		EntityDB(std::string const& filename, Config* config);
		~EntityDB();
		
		//Creates an entity
		bool create_entity(Network::EntityState const& initial_state, EntityID& id);
	
		//Destroys an entity
		bool destroy_entity(EntityID const& id);

		//Retrieves a player entity	
		EntityID get_player_entity_id(std::string const& player_name);
		
	private:
		
		//The entity database
		std::map<EntityID, Entity*>	entities;
	};

};

#endif

