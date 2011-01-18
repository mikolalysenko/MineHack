#ifndef ENTITY_DB_H
#define ENTITY_DB_H

#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>

#include <pthread.h>

#include <tcutil.h>
#include <tctdb.h>

#include "constants.h"
#include "config.h"
#include "chunk.h"
#include "entity.h"

namespace Game
{
	//Entity iterator control (can be OR'd together to create multiple effects)
	enum class EntityUpdateControl : int
	{
		Continue 	= 0,
		Update		= TDBQPPUT,
		Delete		= TDBQPOUT,
		Break		= TDBQPSTOP
	};
	
	//Entity iterator function
	typedef EntityUpdateControl (*entity_update_func)(Entity&, void*);
	
	//Manages a collection of entities
	struct EntityDB
	{
		//Constructor/destructor stuff
		EntityDB(std::string const& filename, Config* config);
		~EntityDB();
		
		//Creates an entity
		bool create_entity(Entity const& initial_state, EntityID& id);
	
		//Destroys an entity
		bool destroy_entity(EntityID const& id);
	
		//Retrieves an entity's state
		bool get_entity(EntityID const& id, Entity& state);
		
		//Updates an entity
		bool update_entity(EntityID const& id, 
			entity_update_func user_func, 
			void* user_data);
	
		//Basic foreach loop on entity db
		bool foreach(
			entity_update_func func, 
			void* data,
			Region const& 	region = Region(), 
			uint8_t			type_filter = 0,
			bool 			only_active = true);
	
		//Retrieves a player (if one exists)
		bool get_player(std::string const& player_name, EntityID& res);
		
	private:
		//The entity database
		TCTDB*	entity_db;
	};
};

#endif


