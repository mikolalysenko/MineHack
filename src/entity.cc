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

using namespace std;

namespace Game
{

void insert_int(TCMAP* map, const char* key, int64_t x)
{
	char buf[64];
	int len = snprintf(buf, 64, "%ld", x);
	tcmapput(map, key, strlen(key), buf, len);
}

void insert_double(TCMAP* map, const char* key, double d)
{
	char buf[128];
	int len = snprintf(buf, 128, "%le", d);
	tcmapput(map, key, strlen(key), buf, len);
}

template<typename T>
	void insert_struct(TCMAP* map, const char* key, T const& s)
{
	tcmapput(map, key, strlen(key), (const void*)&s, sizeof(T));
}

bool get_int(const TCMAP* map, const char* key, int64_t& x)
{
	const char* str = tcmapget2(map, key);
	if(str == NULL)
		return false;
	return sscanf(str, "%ld", &x) > 0;
}

bool get_double(const TCMAP* map, const char* key, double& d)
{
	const char* str = tcmapget2(map, key);
	if(str == NULL)
		return false;
	return sscanf(str, "%le", &d) > 0;
}

template<typename T>
	bool get_struct(const TCMAP* map, const char* key, T& buf)
{
	int vlen;
	const void* data = tcmapget(map, key, strlen(key), &vlen);
	if(data == NULL || vlen != sizeof(T))
		return false;
		
	memcpy((void*)&buf, data, sizeof(T));
	return true;
}

bool MonsterEntity::from_map(const TCMAP* map)
{
	return true;
}

void MonsterEntity::to_map(TCMAP* map) const
{
}

bool PlayerEntity::from_map(const TCMAP* map)
{
	const char* str = tcmapget2(map, "player_name");
	int vsiz = strlen(str);
	if(str == NULL || vsiz > PLAYER_NAME_MAX_LEN)
		return false;
	memcpy(player_name, str, vsiz);
	player_name[vsiz] = '\0';
	
	return true;
}

void PlayerEntity::to_map(TCMAP* map) const
{
	tcmapput2(map, "player_name", player_name);
}


bool EntityBase::from_map(const TCMAP* map)
{
	int64_t tmp;
	if(!get_int(map, "type", tmp))
		return false;
	type = (EntityType)tmp;
	
	if(!get_int(map, "active", tmp))
		return false;
	active = (tmp != 0);

	if(!get_double(map, "x", x))
		return false;	
	if(!get_double(map, "y", y))
		return false;	
	if(!get_double(map, "z", z))
		return false;
		
	if(!get_double(map, "pitch", pitch))
		return false;	
	if(!get_double(map, "yaw", yaw))
		return false;	
	if(!get_double(map, "roll", roll))
		return false;	

	if(!get_int(map, "health", health))
		return false;
	
	return true;
}

void EntityBase::to_map(TCMAP* res) const
{
	insert_int		(res, "type",	(int64_t)type);
	insert_int		(res, "active",	(int64_t)(active != 0));

	insert_double	(res, "x", 		x);
	insert_double	(res, "y", 		y);
	insert_double	(res, "z", 		z);

	insert_double	(res, "pitch", 	pitch);
	insert_double	(res, "yaw", 	yaw);
	insert_double	(res, "roll", 	roll);
	
	insert_int	  	(res, "health",	health);
}

bool Entity::from_map(const TCMAP* res)
{
	if(!base.from_map(res))
		return false;
		
	switch(base.type)
	{
		case EntityType::Player:
			return player.from_map(res);

		case EntityType::Monster:
			return monster.from_map(res);
	}
	
	return true;
}

TCMAP* Entity::to_map() const
{
	TCMAP* res = tcmapnew();
	
	base.to_map(res);
	
	switch(base.type)
	{
		case EntityType::Player:
			player.to_map(res);
		break;
		
		case EntityType::Monster:
			monster.to_map(res);
		break;
	}

	return res;

}


EntityDB::EntityDB(string const& path, Config* config)
{
	//Create db
	entity_db = tctdbnew();
	
	//Set options
	tctdbsetmutex(entity_db);
	
	//Open file
	tctdbopen(entity_db, path.c_str(), TDBOWRITER | TDBOCREAT);
}


EntityDB::~EntityDB()
{
	tctdbclose(entity_db);
	tctdbdel(entity_db);
}

//Creates an entity with the given initial state
bool EntityDB::create_entity(Entity const& initial_state, EntityID& entity_id)
{
	entity_id.id = uuid_gen.gen_uuid();
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
	static const char* AXIS_LABEL = "x\0y\0z";
	static const int COORD_MIN[] = { COORD_MIN_X, COORD_MIN_Y, COORD_MIN_Z };
	static const int COORD_MAX[] = { COORD_MAX_X, COORD_MAX_Y, COORD_MAX_Z };

	//Add query restriction on type flags
	if(type_flags)
	{
		for(int i=1; i<=(int)EntityType::MaxEntityType; i<<=1)
		{
			if(type_flags & i)
			{
				add_query_cond(Q, "type", TDBQCNUMOREQ, i);
			}
		}
	}

	//Add active restriction
	if(only_active)
	{
		tctdbqryaddcond(Q.query, "active", TDBQCSTREQ, "1");
	}
	
	for(int i=0; i<3; i++)
	{
		if(r.lo[i] == COORD_MIN[i] && r.hi[i] == COORD_MAX[i])
			continue;
			
		stringstream ss;
		ss << r.lo[i] << ' ' << r.hi[i];
		tctdbqryaddcond(Q.query, AXIS_LABEL+(i<<1), TDBQCNUMBT, ss.str().c_str());
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

