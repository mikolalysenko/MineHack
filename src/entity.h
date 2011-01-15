#ifndef ENTITY_H
#define ENTITY_H

#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>

#include <pthread.h>

#include <tcutil.h>
#include <tchdb.h>

#include "login.h"
#include "player.h"
#include "chunk.h"

namespace Game
{
	//An entity id
	struct EntityID
	{
		std::uint64_t id;
		
		bool null() const { return id == 0; }
		void clear()	{ id = 0; }
		
		bool operator==(EntityID const& other) const
		{
			return id == other.id;
		}
	};


	//The entity type
	// Must be power of two, are bitwise or'd together to create query flags
	enum class EntityType : std::uint8_t
	{
		NoType	= 0,
		
		Player	= (1 << 0),
		Monster	= (1 << 1),
		
		MaxEntityType
	};
	
	//Player specific entity data
	struct PlayerEntity
	{
		char player_name[PLAYER_NAME_MAX_LEN];
	};
	
	//Monster entity stuff
	struct MonsterEntity
	{
	};
	
	//Common entity data
	struct EntityBase
	{
		//Entity type
		EntityType	type;
	
		//Entity position
		double x, y, z;
		
		//Rotation (stored in Euler angles)
		double pitch, yaw, roll;
		
		//The health of the entity
		int64_t health;
	};
	
	//An entity object
	struct Entity
	{
		EntityID	entity_id;
	
		EntityBase	base;
		
		union
		{
			PlayerEntity	player;
			MonsterEntity	monster;
		};
		
		//Extracts state from a map object
		bool from_map(const TCMAP*);
		
		//Converts the entity into a map object
		TCMAP*	to_map() const;
	};
	
	//Manages a collection of entities
	struct EntityDB
	{
		//Constructor/destructor stuff
		EntityDB(std::string const& filename);
		~EntityDB();
		
		//Creates an entity
		bool create_entity(Entity const& initial_state, EntityID& id);
	
		//Destroys an entity
		bool destroy_entity(EntityID const& id);
	
		//Retrieves an entity's state
		bool get_entity(EntityID const& id, Entity& state);
		
		//Sets the state for an entity
		bool update_entity(EntityID const& id, Entity const& state);
		
		//Retrieves all entities in a region
		std::vector<Entity> search_region(Region const&, uint8_t type_flags);
		
		
		
	private:
		
		//Generates a unique identifier
		EntityID generate_uuid();
		
		//The entity database
		TCTDB*	entity_db;
	};
};

#endif

