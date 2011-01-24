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

EntityDB::EntityDB(string const& path, Config* config) :
	create_handler(NULL),
	update_handler(NULL),
	delete_handler(NULL)
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
	tctdbsetindex(entity_db, "bucket", TDBITLEXICAL | TDBITKEEP);
	tctdbsetindex(entity_db, "player_name", TDBITLEXICAL  | TDBITKEEP);
	tctdbsetindex(entity_db, "type", TDBITLEXICAL | TDBITKEEP);
	tctdbsetindex(entity_db, "active", TDBITLEXICAL | TDBITKEEP);
	tctdbsetindex(entity_db, "poll", TDBITLEXICAL | TDBITKEEP);
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
	ScopeTCMap	M;
	initial_state.to_map(M.map);
	
	bool res = tctdbputkeep(entity_db, &entity_id, sizeof(EntityID), M.map);
	if(res)
		create_handler->call(initial_state);
	
	return res;
}

//Destroys an entity
bool EntityDB::destroy_entity(EntityID const& entity_id)
{
	Entity entity;
	if(!get_entity(entity_id, entity))
		return false;

	bool res = tctdbout(entity_db, &entity_id, sizeof(EntityID));
	if(res)
		delete_handler->call(entity);
	
	return res;
}

//Retrieves an entity
bool EntityDB::get_entity(EntityID const& entity_id, Entity& entity)
{
	entity.entity_id = entity_id;
	
	ScopeTCMap M(tctdbget(entity_db, &entity_id, sizeof(EntityID)));
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
	Entity entity;
	bool created, deleted, updated;

	while(true)
	{
		if(!tctdbtranbegin(entity_db))
		{
			return false;
		}
		
		created = false;
		deleted = false;
		updated = false;
		
		ScopeTCMap M(tctdbget(entity_db, &entity_id, sizeof(EntityID)));
		if(M.map == NULL)
		{
			tctdbtranabort(entity_db);
			return false;
		}
		
		entity.from_map(M.map);
		entity.entity_id = entity_id;
		
		bool palive = !(entity.base.flags & EntityFlags::Inactive);
		int rc = (int)((*callback)(entity, user_data));
		bool nalive = !(entity.base.flags & EntityFlags::Inactive);
		
		if(rc & (int)EntityUpdateControl::Delete)
		{
			if(tctdbout(entity_db, &entity_id, sizeof(EntityID)))
				deleted = true;
		}
		else if(rc & (int)EntityUpdateControl::Update)
		{
			tcmapclear(M.map);
			entity.to_map(M.map);
			tctdbput(entity_db, &entity_id, sizeof(EntityID), M.map);
		
			//Check state transition
			if(nalive && palive)
			{
				updated = true;
			}
			else if(nalive && !palive)
			{
				created = true;
			}
			else if(!nalive && palive)
			{
				deleted = true;
			}
		}
		
		if(tctdbtrancommit(entity_db))
		{
			break;
		}
	}
	
	if(updated)		update_handler->call(entity);
	if(deleted)		delete_handler->call(entity);
	if(created)		create_handler->call(entity);
	
	return true;
}



//Retrieves all entities in a list
bool EntityDB::foreach(
	entity_update_func	user_func,
	void*				user_data,
	Region const& 		r, 
	vector<EntityType>	types,
	bool				only_active)
{
	ScopeTCQuery Q(entity_db);
	
	//Add range query
	add_range_query(Q.query, r);
	
	//Add query restriction on type flags
	if(types.size() > 0)
	{
		stringstream ss;
		for(int i=0; i<types.size(); i++)
		{
			ss << (int)(types[i]) << ' ';
		}
		tctdbqryaddcond(Q.query, "type", TDBQCSTROREQ, ss.str().c_str()); 
	}

	//Add active restriction
	if(only_active)
	{
		tctdbqryaddcond(Q.query, "active", TDBQCSTREQ, "1");
	}
	
	//Define the call back structure locally
	struct UserDelegate
	{
		void* 				context;
		entity_update_func 	func;
		EntityEventHandler	*create_handler, *update_handler, *delete_handler;
		
		
		static int call(const void* key, int ksize, TCMAP* columns, void* data)
		{
			//Extract entity
			Entity entity;
			entity.entity_id = *((const EntityID*)key);
			entity.from_map(columns);
			
			bool palive = !(entity.base.flags & EntityFlags::Inactive);

			//Call user function
			UserDelegate* dg = (UserDelegate*)data;
			int rc = (int)dg->func(entity, dg->context);
			
			bool nalive = !(entity.base.flags & EntityFlags::Inactive);

			
			//If executing update, then need to pack entity into columns
			if(rc & TDBQPOUT)
			{
				dg->delete_handler->call(entity);
			}
			else if(rc & TDBQPPUT)
			{
				tcmapclear(columns);
				entity.to_map(columns);
				
				if(nalive && palive)
				{
					dg->update_handler->call(entity);
				}
				else if(nalive && !palive)
				{
					dg->create_handler->call(entity);
				}
				else if(!nalive && palive)
				{
					dg->delete_handler->call(entity);
				}
			}
			
			return rc;
		}

	};

	//Construct delegate
	UserDelegate dg = { 
		user_data, 
		user_func, 
		create_handler, 
		update_handler, 
		delete_handler };
	
	//Execute
	bool res = tctdbqryproc(Q.query, UserDelegate::call, &dg);
	
	//Print hint string	
	//cout << "Hint: " << tctdbqryhint(Q.query) << endl;

	return res;
}

void EntityDB::add_range_query(TDBQRY* query, Region const& r)
{
	if(r.lo[0] <= 0)
		return;
	
	int bmin_x = (int)(r.lo[0] / BUCKET_X),
		bmin_y = (int)(r.lo[1] / BUCKET_Y),
		bmin_z = (int)(r.lo[2] / BUCKET_Z),
		bmax_x = (int)(r.hi[0] / BUCKET_X),
		bmax_y = (int)(r.hi[1] / BUCKET_Y),
		bmax_z = (int)(r.hi[2] / BUCKET_Z);
		
	//String size
	int size =	(bmax_x - bmin_x + 1) *
				(bmax_y - bmin_y + 1) *
				(bmax_z - bmin_z + 1) * 
				(3 * BUCKET_STR_LEN + 1);
	
	//The bucket string
	ScopeFree guard(NULL);
	char* bucket_str = (char*)alloca(size);
		
	//Insufficient stack space, need to do malloc
	if(bucket_str == NULL)
	{
		bucket_str = (char*)malloc(size);
		guard.ptr = bucket_str;
	}
	
	char* ptr = bucket_str;
	
	for(int bx=bmin_x; bx<=bmax_x; bx++)
	for(int by=bmin_y; by<=bmax_y; by++)
	for(int bz=bmin_z; bz<=bmax_z; bz++)
	{
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
		
		*(ptr++) = ' ';
	}
	
	*(--ptr) = '\0';
	
	tctdbqryaddcond(query, "bucket", TDBQCSTROREQ, bucket_str);
	
	//Add fine grained range conditions to query
	static const char* AXIS_LABEL[] = { "x", "y", "z" };
	for(int i=0; i<3; i++)
	{
		stringstream ss;
		ss << r.lo[i] << ' ' << r.hi[i];
		tctdbqryaddcond(query, AXIS_LABEL[i], TDBQCNUMBT, ss.str().c_str());
	}
}

//Retrieves a player (if one exists)
bool EntityDB::get_player(std::string const& player_name, EntityID& res)
{
	ScopeTCQuery Q(entity_db);
	
	tctdbqryaddcond(Q.query, "player_name", TDBQCSTREQ, player_name.c_str());
	
	ScopeTCList L(tctdbqrysearch(Q.query));
	
	assert(tclistnum(L.list) <= 1);
	
	int sz = 0;
	const void* ptr = tclistval(L.list, 0, &sz);
	if(ptr == NULL)
		return false;
		
	res = *((EntityID*)ptr);
	
	return true;
}

};
