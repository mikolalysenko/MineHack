#ifndef UPDATE_EVENT_H
#define UPDATE_EVENT_H

namespace Game
{
	enum class UpdateEventType
	{
		SetBlock
	};
	
	struct UpdateBlockEvent
	{
		int x, y, z;
		Block b;
	};
	
	struct UpdateEvent
	{
		UpdateEventType type;
		
		union
		{
			UpdateBlockEvent block_event;
		};
	};
};

#endif

