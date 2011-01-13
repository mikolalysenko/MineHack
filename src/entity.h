#ifndef ENTITY_H
#define ENTITY_H

#include <string>
#include <cstdint>
#include <cstdlib>

#include <pthread.h>

#include <tcutil.h>
#include <tchdb.h>


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
	enum class EntityType : std::uint8_t
	{
		Player	= 1,
		Monster	= 2,
	};
	
	//Player specific entity data
	struct PlayerEntity
	{
	};
	
	//Monster entity stuff
	struct MonsterEntity
	{
	};
	
	//Common entity data
	struct EntityHeader
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
		EntityHeader header;
		
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
		
		
	private:
		
		//Generates a unique identifier
		EntityID generate_uuid();
		
		//The entity database
		TCTDB*	entity_db;
	};
};

#endif

