

//Entity constructor
Entity = function(packet)
{
	//Initialize the entity from a packet
}

//Updates entity state from network packet
Entity.prototype.update = function(packet)
{
}

//Updates client model of entity behavior
Entity.prototype.tick = function()
{
}

//Draws the entity
Entity.prototype.draw = function(gl)
{
}



//The entity database
EntityDB = 
{
	index : {}
};


//Initialize entity database
EntityDB.init = function()
{
	EntityDB.index = {};
}

//Kill entity database
EntityDB.shutdown = function()
{
}

//Update an entity using the given network packet
EntityDB.update_entity = function(entity_id, packet)
{
	var ent = EntityDB.index[entity_id];
	if(ent)
	{
		ent.update(packet);
	}
	else
	{
		EntityDB.index[entity_id] = new Entity(packet);
	}
}

//Destroy an entity
EntityDB.destroy_entity = function(entity_id)
{
	if(entity_id in EntityDB.index)
	{
		delete EntityDB.index[entity_id];
	}
}

//Tick all entities
EntityDB.tick = function()
{
	for(i in EntityDB.index)
	{
		EntityDB.index[i].tick();
	}
}

//Draws all entities
EntityDB.draw = function(gl, cam)
{
	for(i in EntityDB.index)
	{
		EntityDB.index[i].draw(gl);
	}
}
