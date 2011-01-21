#ifndef ENTITY_H
#define ENTITY_H

#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>

#include <tcutil.h>

#include "constants.h"
#include "config.h"
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
		Player	= 1,
		Monster = 2,
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
	
	//Entity control flags
	struct EntityFlags
	{
		//Toggles entity state, set to 0 to disable entity 
		// (used for logging player off)
		const static int Inactive		= (1 << 0);
		
		//If set, will post update to entity constantly.  Expensive.
		//Only use with players, MOBs, other highly dynamic, net-relevant entity
		//Other entities will instead be updated on a per-event basis
		const static int Poll			= (1 << 1);
		
		//If set, then the client has no authority to delete this entity
		// Use for MOBs, players, static objects, etc. 
		// Do not use for projectiles, temporary objects etc.
		const static int Persist 		= (1 << 2);
		
		//If set, then only the creation event for this entity is sent to the
		//client.  This is used for projectiles or other short lived objects.
		const static int Temporary		= (1 << 3);
		
	};
	
	//Common entity data
	struct EntityBase
	{
		//Entity type
		EntityType	type;
		
		//If set, entity is active (ie recieves updates, etc.)
		int			flags;
		
		//Entity position
		double x, y, z;
		
		//Rotation (stored in Euler angles)
		double pitch, yaw, roll;
		
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
		void 	to_map(TCMAP*) const;
	};
	
};

#endif

