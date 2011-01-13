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
template<typename T>
bool inventory_transaction(
	InventoryID const& inv_id, 
	T func)
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
	int				index;			//Position of item in inventory
	InventoryID		inv_id;
	InventoryHeader	inv_header;
	int				n_items;
	ItemID*			items;
	int				size;
	void*			data;
};


//Executes an update transaction atomically on an item record and its associated inventory record
template<typename T>
bool item_transaction(
	ItemID const& item_id,
	T func)
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


		//Find position of item in inventory
		bool orphan = true;
		for(int i=0; i<row.n_items; i++)
		{
			if(row.items[i] == item_id)
			{
				row.index = i;
				orphan = false;
				break;
			}
		}
		
		if(orphan)
		{
			//Orphan item exists?
			cout << "This should never happen!  Got orphan item id " << item_id.id << endl;
			tchdbtranabort(inventory_db);
			return false;
		}
		
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
bool create_inventory(InventoryID& result, int capacity)
{
	InventoryID id = generate_inventory_id();

	if(capacity > 0)
	{
		//Allocate data
		int size 	= sizeof(ItemID) * capacity + sizeof(InventoryHeader);
		void * data = calloc(size, 1);
		
		//Set scope guard
		ScopeFree	guard(data);
		
		//Initialize header
		((InventoryHeader*)data)->capacity = capacity;
		
		tchdbput(inventory_db,
			(const void*)&id, sizeof(InventoryID),
			(const void*)&data, size);
	}
	else
	{
		InventoryHeader header;
		header.capacity = capacity;
		
		tchdbput(inventory_db,
			(const void*)&id, sizeof(InventoryID),
			(const void*)&header, sizeof(InventoryHeader));
	}
		
	result = id;
	return true;
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


//Deletes an inventory object
struct InventoryDeleteTransaction
{
	bool operator()(InventoryTransactionRecord& row)
	{
		for(int i=0; i<row.n_items; i++)
		{
			if(!row.items[i].empty())
				tchdbout(inventory_db, (const void*)&row.items[i], sizeof(ItemID));
		}
		
		tchdbout(inventory_db, (const void*)&row.inv_id, sizeof(InventoryID));
		return true;
	}
};

bool delete_inventory(InventoryID const& inv_id)
{
	return inventory_transaction(inv_id, InventoryDeleteTransaction());
}


//Adds an item to the given inventory
struct CreateItemTransaction
{
	ItemID		item_id;
	ItemHeader* item_header;
	
	CreateItemTransaction(
		ItemID const&	id,
		ItemHeader* 	hdr) : 
			item_id(id), item_header(hdr) {}

	bool operator()(InventoryTransactionRecord& row)
	{
		if(row.inv_header.capacity < 0)
		{
			//For unlimited capacity inventory, just append
			tchdbputcat(inventory_db,
				(const void*)&(item_header->inv_id),	sizeof(InventoryID),
				(const void*)&item_id,					sizeof(ItemID));
		}
		else
		{
			//Otherwise, need to put in first available slot (if it exists)
			bool full = true;
		
			for(int i=0; i<row.n_items; i++)
			{
				if(row.items[i].empty())
				{
					row.items[i] = item_id;
					full = false;
					break;
				}
			}
			
			if(full)
				return false;
			
			tchdbput(inventory_db,
				(const void*)&(item_header->inv_id), 	sizeof(InventoryID),
				row.data, 								row.size);
		}
		
		tchdbput(inventory_db,
			(const void*)&item_id,	 	sizeof(ItemID),
			(const void*)item_header, 	sizeof(ItemHeader));
			
		return true;
	
	}
};

bool create_item(InventoryID const& inv_id, Item const& val, ItemID& item_id)
{
	item_id = generate_item_id();
	
	ItemHeader item_header;
	item_header.inv_id	= inv_id;
	item_header.item 	= val;
	
	bool result = inventory_transaction(inv_id, CreateItemTransaction(item_id, &item_header));
	
	//If item creation failed, just return a null item
	if(!result)
		item_id.clear();
	return result;
}


//Retrieves the inventory id associated to a particular item
bool get_item_val(ItemID const& item_id, Item& result)
{
	if(!item_id.valid())
		return false;

	ItemHeader header;
	
	int size = tchdbget3(inventory_db, 
		(const void*)&item_id, 	sizeof(ItemID),
		(void*)&header, 		sizeof(ItemHeader));
		
	if(size != sizeof(ItemHeader))
		return false;
	
	result = header.item;
	
	return true;
}

//Retrieves an item
bool get_item_inventory(ItemID const& item_id, InventoryID& result)
{
	if(!item_id.valid())
		return false;
	
	ItemHeader header;
	
	int size = tchdbget3(inventory_db,
		(const void*)&item_id, 	sizeof(ItemID),
		(void*)&header, 		sizeof(ItemHeader));
		
	if(size != sizeof(ItemHeader))
		return false;
	
	result = header.inv_id;
	
	return true;
}

//Deletes an item
struct DeleteItemTransaction
{
	bool operator()(ItemTransactionRecord& row)
	{
		//Have to remove item from inventory list, two basic cases to deal with
		if(row.inv_header.capacity < 0)
		{
			//Unlimited capacity: Move last item to this index and decrease size
			row.items[row.index] = row.items[row.n_items - 1];
			tchdbput(inventory_db,
				(const void*)&row.inv_id,	sizeof(InventoryID),
				row.data, 					row.size - sizeof(ItemID));
		}
		else
		{
			//Fixed size list, just 0 appropriate item
			row.items[row.index].clear();
			tchdbput(inventory_db,
				(const void*)&row.inv_id,	sizeof(InventoryID),
				row.data, 					row.size);
		}
			
		tchdbout(inventory_db, 
			(const void*)&row.item_id, 	sizeof(ItemID));
	
	}
};

bool delete_item(ItemID const& item_id)
{
	return item_transaction(item_id, DeleteItemTransaction());
}


//Uses an item's charge.  If charge drops to 0, destroys the item.  If item doesn't have enough charges, returns false.
struct UseChargeTransaction
{
	InventoryID	owner_inv_id;
	int 		charge_cost;
	
	UseChargeTransaction(InventoryID const& owner, int ncharge) :
		owner_inv_id(owner), charge_cost(ncharge) {}
		
	
	bool operator()(ItemTransactionRecord& row)
	{
		if(!(row.item_header.inv_id == owner_inv_id))
			return false;
			
		if(charge_cost > row.item_header.item.charges)
		{
			return false;
		}
		else if(charge_cost == row.item_header.item.charges)
		{
			//Charge cost drops to exactly 0, delete object
			DeleteItemTransaction rmtran;
			return rmtran(row);
		}
		else
		{
			row.item_header.item.charges -= charge_cost;
			
			tchdbput(inventory_db,
				(const void*)&row.item_id,		sizeof(ItemID),
				(const void*)&row.item_header,	sizeof(ItemHeader));
		}
	}
	
};

bool use_item_charge(InventoryID const& owner_id, ItemID const& item_id, int cost)
{
	return item_transaction(item_id, UseChargeTransaction(owner_id, cost));
}


}




