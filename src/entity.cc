#include <pthread.h>

#include <cstring>
#include <utility>
#include <algorithm>
#include <iterator>
#include <iostream>
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
	header.type = (EntityType)type;

	if(!get_double(map, "x", header.x))
		return false;	
	if(!get_double(map, "y", header.y))
		return false;	
	if(!get_double(map, "z", header.z))
		return false;
		
	if(!get_double(map, "pitch", header.pitch))
		return false;	
	if(!get_double(map, "yaw", header.yaw))
		return false;	
	if(!get_double(map, "roll", header.roll))
		return false;	

	if(!get_int(map, "health", header.health))
		return false;


	switch(header.type)
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

	insert_int		(res, "type",	(int64_t)header.type);

	insert_double	(res, "x", 		header.x);
	insert_double	(res, "y", 		header.y);
	insert_double	(res, "z", 		header.z);

	insert_double	(res, "pitch", 	header.pitch);
	insert_double	(res, "yaw", 	header.yaw);
	insert_double	(res, "roll", 	header.roll);
	
	insert_int	  	(res, "health",	header.health);
	
	switch(header.type)
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

EntityID EntityDB::generate_uuid()
{
	EntityID res;
	res.id = rand();
	return res;
}


};

