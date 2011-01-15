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

#include "misc.h"
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


bool Entity::from_map(const TCMAP* map)
{
	int64_t type;
	if(!get_int(map, "type", type))
		return false;
	base.type = (EntityType)type;

	if(!get_double(map, "x", base.x))
		return false;	
	if(!get_double(map, "y", base.y))
		return false;	
	if(!get_double(map, "z", base.z))
		return false;
		
	if(!get_double(map, "pitch", base.pitch))
		return false;	
	if(!get_double(map, "yaw", base.yaw))
		return false;	
	if(!get_double(map, "roll", base.roll))
		return false;	

	if(!get_int(map, "health", base.health))
		return false;


	switch(base.type)
	{
		case EntityType::Player:
			if(!get_struct(map, "data", player))
				return false;
		break;
		
		case EntityType::Monster:
			if(!get_struct(map, "data", monster))
				return false;
		break;
	}
	
	return true;
}

TCMAP* Entity::to_map() const
{
	TCMAP* res = tcmapnew();

	insert_int		(res, "type",	(int64_t)base.type);

	insert_double	(res, "x", 		base.x);
	insert_double	(res, "y", 		base.y);
	insert_double	(res, "z", 		base.z);

	insert_double	(res, "pitch", 	base.pitch);
	insert_double	(res, "yaw", 	base.yaw);
	insert_double	(res, "roll", 	base.roll);
	
	insert_int	  	(res, "health",	base.health);
	
	switch(base.type)
	{
		case EntityType::Player:
			insert_struct(res, "data", player);
		break;
		
		case EntityType::Monster:
			insert_struct(res, "data", monster);
		break;
	}
	
	return res;
}

EntityDB::EntityDB(string const& path)
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
	entity_id = generate_uuid();
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

//Generates a unique identifier
EntityID EntityDB::generate_uuid()
{
	EntityID res;
	res.id = rand();
	return res;
}


struct UserDelegate
{
	void* 				context;
	EntityIterFunc 		func;
};

int query_list_translator(const void* key, int ksize, TCMAP* columns, void* data)
{
	//Extract entity
	Entity entity;
	entity.entity_id = *((const EntityID*)key);
	entity.from_map(columns);

	UserDelegate* dg = (UserDelegate*)data;
	return (int)dg->func(entity, dg->context);
}

//Retrieves all entities in a list
bool EntityDB::foreach_region(
	EntityIterFunc	user_func,
	void*			user_data,
	Region const& 	r, 
	uint8_t 		type_flags)
{
	ScopeTCQuery Q(entity_db);
	
	//Add additional query restriction on type flags
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
	
	//Add conditions to query
	add_query_cond(Q, "x", TDBQCNUMGE, r.lo[0]);
	add_query_cond(Q, "x", TDBQCNUMLE, r.hi[0]);
	add_query_cond(Q, "y", TDBQCNUMGE, r.lo[1]);
	add_query_cond(Q, "y", TDBQCNUMLE, r.hi[1]);
	add_query_cond(Q, "z", TDBQCNUMGE, r.lo[2]);
	add_query_cond(Q, "z", TDBQCNUMLE, r.hi[2]);
	
	//Run the delegate
	UserDelegate dg = { user_data, user_func };
	return tctdbqryproc(Q.query, query_list_translator, (void*)&dg);
}

};

