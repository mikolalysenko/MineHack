#ifndef ACTION_H
#define ACTION_H

#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>


#include "constants.h"
#include "config.h"
#include "chunk.h"
#include "entity.h"

namespace Game
{
	enum class ActionType : uint8_t
	{
		DigStart 		= 0,
		DigStop			= 1,
		UseItem			= 2,
	};
	
	//Action target type
	struct ActionTarget
	{
		//Information about action target
		static const int None		= 0;
		static const int Block		= 1;
		static const int Entity		= 2;
		static const int Ray		= 3;
	};
	
	//A player action struct
	struct Action
	{
		ActionType		type;
		int				target_type;
		uint64_t		tick;
		
		union
		{
			struct
			{
				int x, y, z;
			} target_block;
			
			EntityID	target_id;
			
			struct
			{
				double	ox, oy, oz,
						dx, dy, dz;
			} target_ray;
		};
	};

};

#endif
