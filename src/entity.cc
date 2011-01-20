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

namespace Game
{

void bucket_str(double x, double y, double z, char* res)
{
	int ix = (int)(x / BUCKET_X),
		iy = (int)(y / BUCKET_Y),
		iz = (int)(z / BUCKET_Z);

	for(int i=0; i<BUCKET_STR_LEN; i++)
	{
		*(res++) = '0' + (ix & BUCKET_MASK_X);
		ix >>= BUCKET_SHIFT_X;
	}

	for(int i=0; i<BUCKET_STR_LEN; i++)
	{
		*(res++) = '0' + (iy & BUCKET_MASK_Y);
		iy >>= BUCKET_SHIFT_Y;
	}
	
	for(int i=0; i<BUCKET_STR_LEN; i++)
	{
		*(res++) = '0' + (iz & BUCKET_MASK_Z);
		iz >>= BUCKET_SHIFT_Z;
	}
		
	
	*(res++) = 0;
}


void insert_int(TCMAP* map, const char* key, int64_t x)
{
	char buf[64];
	int len = snprintf(buf, 64, "%lld", x);
	tcmapput(map, key, strlen(key), buf, len);
}

void insert_uint(TCMAP* map, const char* key, uint64_t x)
{
	char buf[64];
	int len = snprintf(buf, 64, "%llu", x);
	tcmapput(map, key, strlen(key), buf, len);
}

void insert_double(TCMAP* map, const char* key, long double d)
{
	char buf[128];
	int len = snprintf(buf, 128, "%lf", d);
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
	return sscanf(str, "%lld", &x) > 0;
}

bool get_uint(const TCMAP* map, const char* key, uint64_t& x)
{
	const char* str = tcmapget2(map, key);
	if(str == NULL)
		return false;
	return sscanf(str, "%llu", &x) > 0;
}

bool get_double(const TCMAP* map, const char* key, double& d)
{
	const char* str = tcmapget2(map, key);
	if(str == NULL)
		return false;
	return sscanf(str, "%lf", &d) > 0;
}



bool get_float(const TCMAP* map, const char* key, float& d)
{
	const char* str = tcmapget2(map, key);
	if(str == NULL)
		return false;
	return sscanf(str, "%f", &d) > 0;
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
	
	insert_uint(map, "net_last_tick", net_last_tick);
	insert_double(map, "net_x", net_x);
	insert_double(map, "net_y", net_y);
	insert_double(map, "net_z", net_z);
	insert_double(map, "net_pitch", net_pitch);
	insert_double(map, "net_yaw", net_yaw);
	insert_double(map, "net_roll", net_roll);
	insert_int(map, "net_input", net_input);
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
	
	if(active)
	{
		//Add position bucket field, used for optimizing range searches
		char str[20];
		bucket_str(x, y, z, str);
		tcmapput2(res, "bucket", str);
	}
	

	insert_double	(res, "x", 		x);
	insert_double	(res, "y", 		y);
	insert_double	(res, "z", 		z);

	insert_double	(res, "pitch", 	pitch);
	insert_double	(res, "yaw", 	yaw);
	insert_double	(res, "roll", 	roll);
	
	insert_int	  	(res, "health",	(unsigned)health);
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


};

