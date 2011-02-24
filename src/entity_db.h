#ifndef ENTITY_DB_H
#define ENTITY_DB_H

#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>

#include <pthread.h>

#include <tcutil.h>
#include <tctdb.h>

#include "constants.h"
#include "config.h"
#include "chunk.h"
#include "entity.h"
#include "heartbeat.h"

namespace Game
{
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
	
	//Entity update handler
	struct EntityEventHandler
	{
		virtual void call(Entity const& ev) = 0;
	};
	
	
};

#endif


