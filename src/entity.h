#ifndef ENTITY_H
#define ENTITY_H

#include <vector>
#include <string>
#include <stdint.h>
#include <cstdlib>

#include "constants.h"
#include "config.h"

#include "network.pb.h"

namespace Game
{
	//An entity ID
	typedef uint64_t EntityID;

	//The entity state type
	struct Entity
	{
		//Entity state goes here
	};

	//Manages a collection of entities
	struct EntityDB
	{
		//Constructor/destructor stuff
		EntityDB(Config* config);
		~EntityDB();
		
		void sync();
		
		/*
		bool create_entity(Network::EntityState const& initial_state, EntityID& entity_id);
		void destroy_entity(EntityID const& entity_id);

		EntityID get_player_entity_id(std::string const& player_name);
		*/
		
	private:
		
		Config* config;
		
		//The entity database
		TCMAP*	entities;
	};

};

#endif

