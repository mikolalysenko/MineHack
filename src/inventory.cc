#include <pthread.h>

#include <functional>
#include <utility>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <cstdint>

#include <tcutil.h>
#include <tchdb.h>

#include "misc.h"
#include "inventory.h"

using namespace std;

namespace Game
{

TCHDB*	inventory_db;

struct InventoryHeader
{
	int capacity;
};

struct ItemHeader
{
	InventoryID inv_id;
	Item		item;
};


//Generates a unique id
//TODO: Do something smarter here
uint64_t generate_uuid()
{
	return rand();	
}

//Generates an item id
ItemID generate_item_id()
{
	ItemID res;
	res.id = generate_uuid() & (~INVENTORY_ID_MASK);
	return res;
}

//Generates an inventory id
InventoryID generate_inventory_id()
{
	InventoryID res;
	res.id = generate_uuid() | INVENTORY_ID_MASK;
	return res;
}

//Initialize database
void init_inventory(std::string const& path)
{
	//Create db
	inventory_db = tchdbnew();
	
	//Set options
	tchdbsetcache(inventory_db, (1<<16));
	tchdbsetmutex(inventory_db);
	
	//Open file
	tchdbopen(inventory_db, path.c_str(), HDBOWRITER | HDBOCREAT);
}

//Shutdown database
void shutdown_inventory()
{
	tchdbclose(inventory_db);
	tchdbdel(inventory_db);
}


/*

//Passed to client function
struct InventoryTransactionRecord
{
	InventoryID		inv_id;
	InventoryHeader	inv_header;
	int				n_items;
	ItemID*			items;
	int				size;
	void*			data;
};


//Executes a transaction on an inventory record atomically
bool inventory_transaction(
	InventoryID const& inv_id, 
	std::function< bool(InventoryTransactionRecord&) > func)
{
	if(!inv_id.valid())
		return false;

	while(true)
	{
		if(!tchdbtranbegin(inventory_db))
			return false;
			
		int size;
		void* data = tchdbget(inventory_db, 
			(const void*)&inv_id, 	sizeof(InventoryID), &size);
		
		if(data == NULL)
		{
			tchdbtranabort(inventory_db);
			return false;
		}
		
		ScopeFree guard(data);
		
		//Build row
		InventoryTransactionRecord row;
		row.inv_id		= inv_id;
		row.inv_header	= *((InventoryHeader*)data);
		row.n_items		= (size - sizeof(InventoryHeader)) / sizeof(ItemID);
		row.items		= (ItemID*)(void*)((uint8_t*)data + sizeof(ItemHeader));
		row.size		= size;
		row.data		= data;
		
		
		if( !func(row) )
		{
			tchdbtranabort(inventory_db);
			return false;
		}
		
		if(tchdbtrancommit(inventory_db))
			return true;
	}
}

//Passed to client function
struct ItemTransactionRecord
{
	ItemID			item_id;
	ItemHeader		item_header;
	InventoryID		inv_id;
	InventoryHeader	inv_header;
	int				n_items;
	ItemID*			items;
	int				size;
	void*			data;
};


//Executes an update transaction atomically on an item record and its associated inventory record
bool item_transaction(
	ItemID const& item_id,
	std::function< bool(ItemTransactionRecord&) > func)
{
	if(!item_id.valid() || item_id.empty())
		return false;

	while(true)
	{
		if(!tchdbtranbegin(inventory_db))
			return false;
			
		ItemHeader item_header;
		int ih_size = tchdbget3(inventory_db, 
			(const void*)&item_id,	sizeof(InventoryID), 
			(void*)&item_header, 	sizeof(item_header));
		
		//Validate item header record
		if(ih_size != sizeof(ItemHeader) || !item_header.inv_id.valid())
		{
			tchdbtranabort(inventory_db);
			return false;
		}
		
		//Read inventory header
		int size;
		void* data = tchdbget(inventory_db, (const void*)&item_header.inv_id, sizeof(InventoryID), &size);
		
		//Validate inventory record
		if(data == NULL)
		{
			tchdbtranabort(inventory_db);
			return false;
		}
		
		//Set scope guard
		ScopeFree guard(data);
		
		//Create the transaction row
		ItemTransactionRecord row;
		row.item_id 		= item_id;
		row.item_header		= item_header;
		row.inv_header		= *((InventoryHeader*)data);
		row.n_items			= (size - sizeof(InventoryHeader)) / sizeof(ItemID);
		row.items			= (ItemID*)(void*)((uint8_t*)data + sizeof(ItemHeader));
		row.size			= size;
		row.data 			= data;
		
		//Invoke update function
		if( !func(row) )
		{
			tchdbtranabort(inventory_db);
			return false;
		}
		
		//Commit transaction
		if(tchdbtrancommit(inventory_db))
			return true;
	}
}


//Creates an inventory object
//	If capacity is -1, then the number of items is infinite
//	Otherwise the inventory is a fixed size
InventoryID create_inventory(int capacity)
{
	InventoryID id = generate_inventory_id();

	if(capacity > 0)
	{
		//Allocate data
		int size 	= sizeof(ItemID) * capacity + sizeof(InventoryHeader);
		void * data = calloc(size, 1);
		
		//Initialize header
		((InventoryHeader*)data)->capacity = capacity;
		
		tchdbput(inventory_db,
			(const void*)&id, sizeof(InventoryID),
			(const void*)&data, size);
			
		free(data);
	}
	else
	{
		InventoryHeader header;
		header.capacity = capacity;
		
		tchdbput(inventory_db,
			(const void*)&id, sizeof(InventoryID),
			(const void*)&header, sizeof(InventoryHeader));
	}
		
	return id;
}

//Returns the items contained in a particular inventory object
// Since it is just a read, don't need a transaction
bool get_inventory(InventoryID const& id, vector<ItemID>& result)
{
	if(!id.valid())
		return false;

	int size;
	void * data = tchdbget(inventory_db, &id, sizeof(id), &size);

	if(data == NULL)
		return false;
	
	//Set a scope guard to free the pointer
	ScopeFree guard(data);

	//Strip out header and get item pointer
	int n_items 			= (size - sizeof(InventoryHeader)) / sizeof(ItemID);
	ItemID* items			= (ItemID*)((void*)((uint8_t*)data + sizeof(InventoryHeader)));
	InventoryHeader header	= *((InventoryHeader*)data);
	
	//Make sure inventory is properly sized	
	assert(header.capacity < 0 || header.capacity == n_items);

	
	result.reserve(n_items);
	copy(items, items+n_items, result.begin());
	
	return true;
}


//Swaps two items in an inventory (used when player is rearranging inventory)
bool swap_item(InventoryID const& inv_id, int idx1, int idx2)
{
	return inventory_transaction(inv_id, 
		std::function< bool(InventoryTransactionRecord) >
		([](InventoryTransactionRecord)
	{
		if(	idx1 >= n_items || idx1 < 0 ||
			idx2 >= n_items || idx2 < 0 )
		{
			return false;
		}
		
		ItemID tmp 	= items[idx1];
		items[idx1] = items[idx2];
		items[idx2] = items[idx1];
		
		tchdbput(inventory_db,
			(const void*)&id, 	sizeof(InventoryID),
			(const void*)data, 	size);
		
		return true;
	}));
}


//Deletes an inventory object
bool delete_inventory(InventoryID const& inv_id)
{
	return inventory_transaction(inv_id, [&](
		InventoryHeader const& 	inv_header,
		int 					n_items, 
		ItemID* 				items,
		int 					size,
		void* 					data)
	{
		for(int i=0; i<n_items; i++)
		{
			if(!items[i].empty())
				tchdbout(inventory_db, (const void*)&items[i], sizeof(ItemID));
		}
		
		tchdbout(inventory_db, (const void*)&id, sizeof(InventoryID));
	});
}



//Adds an item to the given inventory
ItemID create_item(InventoryID const& inv_id, Item const& val)
{
	ItemID item_id = generate_item_id();

	ItemHeader item_header;
	item_header.inv_id 	= inv_id;
	item_header.item 	= val;
	
	bool result = inventory_transaction(inv_id, [&](
		InventoryHeader const&	inv_header,
		int 					n_items, 
		ItemID* 				items,
		int						size,
		void* 					data)
	{
		if(inv_header.capacity < 0)
		{
			//For unlimited capacity inventory, just append
			tchdbputcat(inventory_db,
				(const void*)&inv_id,	sizeof(InventoryID),
				(const void*)&item_id,	sizeof(ItemID));
		}
		else
		{
			//Otherwise, need to put in first available slot (if it exists)
			bool full = true;
		
			for(int i=0; i<n_items; i++)
			{
				if(items[i].empty())
				{
					items[i] = res;
					full = false;
					break;
				}
			}
			
			if(full)
				return false;
			
			tchdbput(inventory_db,
				(const void*)&inv_id, 	sizeof(InventoryID),
				data, 					size);
		}
		
		tchdbput(inventory_db,
			(const void*)&item_id, 		sizeof(ItemID),
			(const void*)&item_header, 	sizeof(ItemHeader));
			
		return true;
	});
	
	//If item creation failed, just return a null item
	if(!result)
		item_id.id = 0;
	return item_id;
}

//Retrieves the inventory id associated to a particular item
bool get_item_val(ItemID const& id, Item& result)
{
	assert(id.valid());

	ItemHeader header;
	
	int size = tchdbget(inventory_db, 
		(const void*)&id, 	sizeof(ItemID),
		(void*)&header, 	sizeof(ItemHeader));
		
	if(size != sizeof(ItemHeader))
		return false;
	
	result = header.item;
	
	return true;
}

//Retrieves an item
bool get_item_inventory(ItemID const& item, InventoryID& result)
{
	assert(id.valid());

	ItemHeader header;
	
	int size = tchdbget(inventory_db,
		(const void*)&id, 	sizeof(ItemID),
		(void*)&header, 	sizeof(ItemHeader));
		
	if(size != sizeof(ItemHeader))
		return false;
	
	result = header.inv_id;
	
	return true;
}

//Deletes an item
bool delete_item(ItemID const& item_id)
{
	return item_transaction(item_id, [&](
		ItemHeader const& 		item_header,
		InventoryHeader const&	inv_header,
		int						n_items,
		ItemID* 				items,
		int 					size,
		void* 					data)
	{
		//Have to remove item from inventory list, two basic cases to deal with
		if(inv_header.capacity < 0)
		{
			//Variable size list - move last item to position and decrement length
			bool found_item = false;
			for(int i=0; i<n_items; i++)
			{
				if(items[i] == item)
				{
					items[i] = items[n_items-1];
					found_item = true;
					break;
				}
			}
			if(!found_item)
				return false;
			
			tchdbput(inventory_db,
				(const void*)&inv,	sizeof(InventoryID),
				data, 				size - sizeof(ItemID));
		}
		else
		{
			//Fixed size list, just 0 appropriate item
			bool found_item = false;
			for(int i=0; i<n_items; i++)
			{
				if(items[i] == item)
				{
					items[i].id = 0;
					found_item = true;
					break;
				}
			}
			if(!found_item)
				return false;
			
			tchdbput(inventory_db,
				(const void*)&inv,	sizeof(InventoryID),
				data, 				size);
		}
			
		tchdbout(inventory_db, 
			(const void*)&item, 	sizeof(ItemID));
	});
}

//Transfers an item to a new inventory
bool move_item(ItemID const& item, InventoryID const& source, InventoryID const& target)
{
	return false;
}

//Uses an item's charge.  If charge drops to 0, destroys the item.  If item doesn't have enough charges, returns false.
bool use_item_charge(InventoryID const& owner, ItemID const& item, int ncharge=1)
{
	return false;
}
*/

}
