#ifndef MAP_WORKER_H
#define MAP_WORKER_H

#include <tbb/task.h>

#include "constants.h"
#include "config.h"
#include "chunk.h"
#include "game_map.h"
#include "session.h"

namespace Game
{
	//The map worker queues up chunk updates for a client and broadcasts them out
	struct MapWorker : public tbb::task
	{
		MapWorker();
		virtual ~MapWorker();
		
		//Executes the task
		virtual tbb::task* execute();
		
	private:
		SessionID		session_id;
		SessionManager* session_manager;
		GameMap*		game_map;
	};
};

#endif
