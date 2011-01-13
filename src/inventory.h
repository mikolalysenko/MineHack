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

#define INVENTORY_ID_MASK		(1ULL<<50ULL)

namespace Game
{	

	struct ItemID
	{
		std::uint64_t id;
		
		bool valid() const	{ return (INVENTORY_ID_MASK & id) == 0; }
		bool empty() const	{ return id == 0; }
		void clear()	  	{ id = 0; }
		
		bool operator==(const ItemID& other) const 
		{
			return id == other.id;
		}
	};
	
	struct InventoryID
	{
		std::uint64_t id;
		
		bool valid() const { return (INVENTORY_ID_MASK & id) != 0; }
		
		bool operator==(const InventoryID& other) const 
		{
			return id == other.id;
		}
	};
	
	enum class ItemType : std::uint8_t
	{
		BlockStack	= 1,
		
		//Other item types go here (tools, food, etc.)
	};
	
	struct BlockItem
	{
		Block		block_type;
	};
	
	//An item is an individual object in the game world
	struct Item
	{
		ItemType		type;			//The item type
		int				charges;		//The number of charges associated to this item.
										//   When this reaches 0, destroy the item
		union
		{
			BlockItem	block_stack;
		};
	};
	
	
	//Initialize database
	void init_inventory(std::string const& path);
	
	//Shutdown database
	void shutdown_inventory();


	//Creates an inventory object
	//	If capacity is -1, then the number of items is infinite
	//	Otherwise the inventory is a fixed size
	bool create_inventory(InventoryID&, int capacity = -1);

	//Returns the items contained in a particular inventory object
	bool get_inventory(InventoryID const& id, std::vector<ItemID>&);

	//Deletes an inventory object
	bool delete_inventory(InventoryID const& id);



	//Adds an item to the given inventory
	bool create_item(InventoryID const&, Item const&, ItemID&);

	//Retrieves the inventory id associated to a particular item
	bool get_item_inventory(ItemID const& item, InventoryID&);

	//Retrieves an item
	bool get_item_val(ItemID const& item, Item&);

	//Deletes an item
	bool delete_item(ItemID const& item);

	//Uses an item's charge.  If charge drops to 0, destroys the item.  If item doesn't have enough charges, returns false.
	bool use_item_charge(InventoryID const& owner, ItemID const& item, int ncharge=1);

};

#endif

