

//Entity constructor
Entity = function(
	entity_id,
	x, y, z,
	pitch, yaw, roll)
{
	this.entity_id = entity_id;
	
	this.x = x;
	this.y = y;
	this.z = z;
	
	this.pitch	= pitch;
	this.yaw	= yaw;
	this.roll	= roll;
}

//The entity database
EntityDB = 
{
	index : {},	//The entity index
	count : 0	//Number of entities
};

//Update an entity using the given network packet
EntityDB.update_entity = function(entity_id, packet)
{
}

//Destroy an entity
EntityDB.destroy_entity = function(entity_id)
{
}

//Tick all entities
EntityDB.tick = function()
{
}
