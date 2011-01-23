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

#include "constants.h"
#include "misc.h"
#include "chunk.h"
#include "entity.h"

using namespace std;
using namespace __gnu_cxx;

namespace Game
{

void bucket_str(double x, double y, double z, char* ptr)
{
	int bx = (int)(x / BUCKET_X),
		by = (int)(y / BUCKET_Y),
		bz = (int)(z / BUCKET_Z);

	//cout << "B = " << bx << ',' << by << ',' << bz << endl;

	int t = bx;
	for(int i=0; i<BUCKET_STR_LEN; i++)
	{
		*(ptr++) = '0' + (t & BUCKET_STR_MASK);
		t >>= BUCKET_STR_BITS;
	}
	
	t = by;
	for(int i=0; i<BUCKET_STR_LEN; i++)
	{
		*(ptr++) = '0' + (t & BUCKET_STR_MASK);
		t >>= BUCKET_STR_BITS;
	}

	t = bz;
	for(int i=0; i<BUCKET_STR_LEN; i++)
	{
		*(ptr++) = '0' + (t & BUCKET_STR_MASK);
		t >>= BUCKET_STR_BITS;
	}
	
	*(ptr++) = 0;
	
}

//TODO: This should really be done more efficiently...
//Maybe look into finding a library for fast string conversion, or else manually instantiate optimized versions of these templates...

//Inserts the key as a string
template<typename T>
	void insert_str(TCMAP* map, const char* key, T const& x)
{
	stringstream ss;
	ss << x;
	tcmapput2(map, key, ss.str().c_str());
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
	x = strtol(str, NULL, 10);
	return true;
}

bool get_uint(const TCMAP* map, const char* key, uint64_t& x)
{
	const char* str = tcmapget2(map, key);
	if(str == NULL)
		return false;
	x = strtoul(str, NULL, 10);
	return true;
}

bool get_float(const TCMAP* map, const char* key, float& x)
{
	const char* str = tcmapget2(map, key);
	if(str == NULL)
		return false;
	x = strtod(str, NULL);
	return true;
}


bool get_double(const TCMAP* map, const char* key, double& x)
{
	const char* str = tcmapget2(map, key);
	if(str == NULL)
		return false;
	x = strtod(str, NULL);
	return true;
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
	memset(player_name, 0, PLAYER_NAME_MAX_LEN + 1);
	memcpy(player_name, str, vsiz);
	
	get_uint(map, "net_last_tick", net_last_tick);
	get_double(map, "net_x", net_x);
	get_double(map, "net_y", net_y);
	get_double(map, "net_z", net_z);
	get_float(map, "net_pitch", net_pitch);
	get_float(map, "net_yaw", net_yaw);
	get_float(map, "net_roll", net_roll);
	
	int64_t inp;
	get_int(map, "net_input", inp);	
	net_input = inp;
	
	return true;
}

void PlayerEntity::to_map(TCMAP* map) const
{
	tcmapput2(map, "player_name", player_name);
	insert_str(map, "net_last_tick", net_last_tick);
	insert_str(map, "net_x", net_x);
	insert_str(map, "net_y", net_y);
	insert_str(map, "net_z", net_z);
	insert_str(map, "net_pitch", net_pitch);
	insert_str(map, "net_yaw", net_yaw);
	insert_str(map, "net_roll", net_roll);
	insert_str(map, "net_input", net_input);
}


bool EntityBase::from_map(const TCMAP* map)
{
	int64_t tmp;
	if(!get_int(map, "type", tmp))
		return false;
	type = (EntityType)tmp;

	if(!get_int(map, "flags", tmp))
		return false;
	flags = tmp;

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
	
	return true;
}

void EntityBase::to_map(TCMAP* res) const
{
	insert_str		(res, "type",	(int)type);
	
	//Set flags and search strings
	insert_str (res, "active",	(int)((flags & EntityFlags::Inactive) == 0));
	bool poll = (flags & (EntityFlags::Poll | EntityFlags::Inactive)) == EntityFlags::Poll;
	insert_str (res, "poll", 	(int)(poll));
	insert_str (res, "flags",	(int)flags);

	//Add position bucket field, used for optimizing range searches
	char str[20];
	bucket_str(x, y, z, str);
	tcmapput2(res, "bucket", str);
	
	cout << "Updating entity: bucket = " << str << endl;

	

	insert_str(res, "x", 		x);
	insert_str(res, "y", 		y);
	insert_str(res, "z", 		z);

	insert_str(res, "pitch", 	pitch);
	insert_str(res, "yaw", 		yaw);
	insert_str(res, "roll", 	roll);
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

void Entity::to_map(TCMAP* res) const
{
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
}


};

