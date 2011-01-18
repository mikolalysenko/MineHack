#include <pthread.h>

#include <cstring>
#include <utility>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include <tcutil.h>
#include <tctdb.h>

#include "constants.h"
#include "misc.h"
#include "chunk.h"
#include "entity.h"
#include "entity_db.h"

using namespace std;


namespace Game
{

EntityDB::EntityDB(string const& path, Config* config)
{
	//Create db
	entity_db = tctdbnew();
	
	//Set options
	tctdbsetmutex(entity_db);
	tctdbtune(entity_db, (1<<20), 4, 12, TDBTLARGE | TDBTBZIP);
	tctdbsetcache(entity_db, (1<<20), (1<<16), (1<<10));
	
	//Open file
	tctdbopen(entity_db, path.c_str(), TDBOWRITER | TDBOCREAT);

	//Kill a UID to make 0 available
	tctdbgenuid(entity_db);
	
	//Set indices
	tctdbsetindex(entity_db, "x", TDBITDECIMAL);
	tctdbsetindex(entity_db, "y", TDBITDECIMAL);
	tctdbsetindex(entity_db, "z", TDBITDECIMAL);
	
	tctdbsetindex(entity_db, "player_name", TDBITQGRAM);


}


EntityDB::~EntityDB()
{
	tctdbclose(entity_db);
	tctdbdel(entity_db);
}

//Creates an entity with the given initial state
bool EntityDB::create_entity(Entity const& initial_state, EntityID& entity_id)
{
	entity_id.id = (uint64_t)tctdbgenuid(entity_db);
	return tctdbputkeep(entity_db, 
		(const void*)&entity_id, sizeof(EntityID), 
		initial_state.to_map());
}

//Destroys an entity
bool EntityDB::destroy_entity(EntityID const& entity_id)
{
	return tctdbout(entity_db, (const void*)&entity_id, sizeof(EntityID));
}

//Retrieves an entity
bool EntityDB::get_entity(EntityID const& entity_id, Entity& entity)
{
	entity.entity_id = entity_id;
	
	ScopeTCMap M(tctdbget(entity_db, (const void*)&entity_id, sizeof(EntityID)));
	if(M.map == NULL)
		return false;
	
	entity.from_map(M.map);
	return true;
}

//Updates an entity in place
bool EntityDB::update_entity(
	EntityID const& entity_id, 
	entity_update_func callback, 
	void* user_data)
{
	while(true)
	{
		if(!tctdbtranbegin(entity_db))
		{
			return false;
		}
		
		
		ScopeTCMap M(tctdbget(entity_db, &entity_id, sizeof(EntityID)));
		if(M.map == NULL)
		{
			tctdbtranabort(entity_db);
			return false;
		}
		
		Entity entity;
		entity.from_map(M.map);
		entity.entity_id = entity_id;
		
		int rc = (int)((*callback)(entity, user_data));
		
		if(rc & (int)EntityUpdateControl::Update)
		{
			cout << "updating entity" << endl;
			ScopeTCMap M(entity.to_map());
			tctdbput(entity_db, &entity_id, sizeof(EntityID), M.map);
		}
		else if(rc & (int)EntityUpdateControl::Delete)
		{
			tctdbout(entity_db, &entity_id, sizeof(EntityID));
		}
		
		if(tctdbtrancommit(entity_db))
			return true;
	}
}



//Retrieves all entities in a list
bool EntityDB::foreach(
	entity_update_func	user_func,
	void*				user_data,
	Region const& 		r, 
	uint8_t 			type_flags,
	bool				only_active)
{
	ScopeTCQuery Q(entity_db);
	
	//Add range conditions to query
	static const char* AXIS_LABEL[] = { "x", "y", "z" };
	static const int COORD_MIN[] = { COORD_MIN_X, COORD_MIN_Y, COORD_MIN_Z };
	static const int COORD_MAX[] = { COORD_MAX_X, COORD_MAX_Y, COORD_MAX_Z };

	//Add query restriction on type flags
	if(type_flags)
	{
		stringstream ss;
	
		for(int i=1; i<=(int)EntityType::MaxEntityType; i<<=1)
		{
			if(type_flags & i)
			{
				if(i != 1)
					ss << ' ';
				ss << i;
			}
		}
		
		tctdbqryaddcond(Q.query, "type", TDBQCNUMOREQ, ss.str().c_str()); 
	}

	//Add active restriction
	if(only_active)
	{
		tctdbqryaddcond(Q.query, "active", TDBQCSTREQ, "1");
	}
	
	for(int i=0; i<3; i++)
	{
		if(r.lo[i] <= COORD_MIN[i] && r.hi[i] >= COORD_MAX[i])
			continue;
			
		stringstream ss;
		ss << r.lo[i] << ' ' << r.hi[i];
		
		tctdbqryaddcond(Q.query, AXIS_LABEL[i], TDBQCNUMBT, ss.str().c_str());
	}
	
	//Define the call back structure locally
	struct UserDelegate
	{
		void* 				context;
		entity_update_func 	func;
		
		static int call(const void* key, int ksize, TCMAP* columns, void* data)
		{
			//Extract entity
			Entity entity;
			entity.entity_id = *((const EntityID*)key);
			entity.from_map(columns);

			UserDelegate* dg = (UserDelegate*)data;
			return (int)dg->func(entity, dg->context);
		}

	};

	//Construct delegate
	UserDelegate dg = { user_data, user_func };
	
	//Execute
	return tctdbqryproc(Q.query, UserDelegate::call, &dg);
}

//Retrieves a player (if one exists)
bool EntityDB::get_player(std::string const& player_name, EntityID& res)
{
	ScopeTCQuery Q(entity_db);
	
	tctdbqryaddcond(Q.query, "player_name", TDBQCSTREQ, player_name.c_str());
	
	ScopeTCList L(tctdbqrysearch(Q.query));
	
	int sz = 0;
	ScopeFree E(tclistpop(L.list, &sz));
	if(sz != sizeof(EntityID))
		return false;
	
	res = *((EntityID*)E.ptr);
	
	return true;
}

};
