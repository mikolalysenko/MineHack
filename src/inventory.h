/*
#ifndef INVENTORY_H
#define INVENTORY_H

#include <pthread.h>

#include <vector>
#include <algorithm>
#include <string>
#include <cstdint>

#include "chunk.h"

namespace Game
{	

	struct ItemID
	{
		std::uint64_t id;
	};
	
	struct InventoryID
	{
		std::uint64_t id;
	};
	
	enum class ItemType : std::uint8_t
	{
		Empty		= 0,
		BlockStack	= 1,
		
		//Other item types go here (tools, food, etc.)
	};
	
	struct BlockItem
	{
		Block		block_type;
	};
	
	struct Item
	{
		ItemType		type;			//The item type
		ItemID			uuid;			//The UUID associated to this item
		InventoryID		inv_uuid;		//The UUID for the inventory containing this item
		int				charges;		//The number of charges associated to this item.
										//   When this reaches 0, destroy the item
		union
		{
			BlockItem	block_stack;
		};
	};
	
	//An inventory tracks a collection of items
	//Provides a set of basic operations for interacting with items that ensure:
	//  1. That each item is associated to a unique inventory
	//		In other words, for all items:
	//			item.inventory contains item
	//	2. That no two items have the same unique identifier
	//	3. No two inventories have the same unqiue identifier
	//	4. Updates on items are done atomically
	struct Inventory
	{
		//The inventory objects
		InventoryID			uuid;
		int					capacity;		
		pthread_spinlock_t	lock;
			
		//Creates an inventory (do not ever call this directly, use create_inventory instead)
		Inventory(InventoryID, int);
	};
	
	//Creates a new inventory
	InventoryID create_inventory(int capacity);
	
	//Recovers an inventory
	Inventory* get_inventory(InventoryID);
	
	//Looks up an item
	Item* get_item(ItemID);
	
	//Locks a set of inventories
	struct InventoryLock
	{
		std::vector<Inventory*> inventories;
		
		InventoryLock(std::vector<Inventory*> inv) : inventories(inv)
		{
			//Sort inventories by UUID
			std::sort(inventories.begin(), inventories.end(),
				[](const Inventory* a, const Inventory* b)
				{
					return a->uuid.id > b->uuid.id;
				});
				
			//Lock in order
			for(auto i=inventories.begin(); i!=inventories.end(); ++i)
			{
				pthread_spinlock_lock((*i)->lock);
			}
		}
		
		~InventoryLock()
		{
			for(auto i=inventories.rbegin(); i!=inventories.rend(); ++i)
			{
				pthread_spinlock_unlock((*i)->lock);
			}
		}

	};
};

#endif
*/
