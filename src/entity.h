#ifndef ENTITY_H
#define ENTITY_H

#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>

#include <pthread.h>

#include <tcutil.h>
#include <tctdb.h>

#include "config.h"
#include "constants.h"
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
		
		bool operator<(EntityID const& other) const
		{
			return id < other.id;
		}
	};


	//The entity type
	// Must be power of two, are bitwise or'd together to create query flags
	enum class EntityType : std::uint8_t
	{
		NoType	= 0,
		
		Player	= (1 << 0),
		Monster = (1 << 1),
		
		MaxEntityType
	};
	
	//Player specific entity data
	struct PlayerEntity
	{
		//Player name
		char		player_name[PLAYER_NAME_MAX_LEN + 1];

		//Network state for player
		uint64_t	net_last_tick;
		double		net_x, net_y, net_z;
		float		net_pitch, net_yaw, net_roll;
		int			net_input;
		
		//Database serialization
		bool 	from_map(const TCMAP*);
		void	to_map(TCMAP*) const;
	};
	
	//Monster entity stuff
	struct MonsterEntity
	{
		//Database serialization
		bool 	from_map(const TCMAP*);
		void	to_map(TCMAP*) const;
	};
	
	//Common entity data
	struct EntityBase
	{
		//Entity type
		EntityType	type;
		
		//If set, entity is active (ie recieves updates, etc.)
		bool active;
		
		//Entity position
		double x, y, z;
		
		//Rotation (stored in Euler angles)
		double pitch, yaw, roll;
		
		//The health of the entity
		int64_t health;
		
		//Database serialization
		bool 	from_map(const TCMAP*);
		void	to_map(TCMAP*) const;
	};
	
	//An entity object
	struct Entity
	{
		//The entity id
		EntityID	entity_id;
	
		//Entity type base information
		EntityBase	base;
		
		union
		{
			PlayerEntity	player;
			MonsterEntity	monster;
		};
		
		//Database serialization
		bool	from_map(const TCMAP*);
		TCMAP*	to_map() const;
	};
	
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
		
		//Generates a unique identifier
		EntityID generate_uuid();
		
		//The entity database
		TCTDB*	entity_db;
	};
};

#endif

