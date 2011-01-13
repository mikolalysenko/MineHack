#ifndef INVENTORY_H
#define INVENTORY_H

#include <pthread.h>

#include <vector>
#include <algorithm>
#include <string>
#include <cstdint>

#include "chunk.h"

//Provides a set of basic operations for interacting with items that ensure:
//
//  1. Each item is associated to a unique inventory
//		In other words, for all items:
//			item.inventory contains item
//	2. That no two items have the same unique identifier
//	3. No two inventories have the same unqiue identifier
//	4. Updates on items are done atomically
//	5. No inventory contains more items than its total capacity
//

#define INVENTORY_CAPACITY		32

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
	struct Inventory
	{
		//The inventory objects
		InventoryID			uuid;

		ItemID				items[INVENTORY_CAPACITY];
	};
	
	//
	InventoryID create_inventory(int capacity);
	
	
	//Moves an item to the target inventory
	bool move_item(ItemID, InventoryID);
	
	//Uses an item
	bool use_charge(ItemID, int count);
};

#endif

