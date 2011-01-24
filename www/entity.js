"use strict";

//Entity constructor
Entity = function(coords, packet, len)
{
	//Set base coordinates (temporary
	this.x = coords[1];
	this.y = coords[2];
	this.z = coords[3];
	this.pitch 	= coords[4];
	this.yaw 	= coords[5];
	this.roll 	= coords[6];
	
	//Set network coordinates
	this.net_t 		= coords[0];
	this.net_x 		= coords[1];
	this.net_y 		= coords[2];
	this.net_z 		= coords[3];
	this.net_pitch	= coords[4];
	this.net_yaw	= coords[5];
	this.net_roll	= coords[6];
	
	//Parse out packet data for initialization
	this.type = packet[len.val++];
	
	if(this.type == PLAYER_ENTITY)
	{
		//Parse out player name from packet
		var name_len = packet[len.val++];
		this.player_name = "";
		for(var i=2; i<2+name_len; i++)
			this.player_name += String.fromCharCode(packet[len.val++] & 0x7f);
		
	}
	else if(this.type == MONSTER_ENTITY)
	{
	}
}

//Updates entity state from network packet
Entity.prototype.update = function(coords, packet)
{
	//Set network coordinates
	this.net_t 		= coords[0];
	this.net_x 		= coords[1];
	this.net_y 		= coords[2];
	this.net_z 		= coords[3];
	this.net_pitch	= coords[4];
	this.net_yaw	= coords[5];
	this.net_roll	= coords[6];
	
	if(this.type == PLAYER_ENTITY)
	{
	
	}
	else if(this.type == MONSTER_ENTITY)
	{
	
	}
}

Entity.prototype.pose_matrix = function()
{
	var cp = Math.cos(this.pitch);
	var sp = Math.sin(this.pitch);
	var cy = Math.cos(this.yaw);
	var sy = Math.sin(this.yaw);
	var cr = Math.cos(this.roll);
	var sr = Math.sin(this.roll);
	
	var rotp = new Float32Array([
		 1,   0,  0, 0,
		 0,  cp, sp, 0,
		 0, -sp, cp, 0,
		 0,   0,  0, 1]); 
		  
	var roty = new Float32Array([
		 cy, 0, sy, 0,
		  0, 1,  0, 0,
		-sy, 0, cy, 0,
		  0, 0,  0, 1]);
		  
	var rotr = new Float32Array([
		cr, sr, 0, 0,
		-sr, cr, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1]);
	
	var rot = mmult(mmult(rotp, roty), rotr);
	
	//Need to shift by player chunk in order to avoid numerical problems
	var c = Player.chunk();	
	c[0] *= CHUNK_X;
	c[1] *= CHUNK_Y;
	c[2] *= CHUNK_Z;
		
	var trans = new Float32Array([
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		c[0]-this.x, c[1]-this.y, c[2]-this.z, 1])
	
	return mmult(rot, trans);
}

//Interpolates local state from last known network state
Entity.prototype.tick = function()
{
	if(this == Player.entity)
	{
		//Only resync player if we are really messed up
		return;
	}
	
	//For now, no interpolation just set position
	this.x		= this.net_x;
	this.y		= this.net_y;
	this.z		= this.net_z;
	this.pitch	= this.net_pitch;
	this.yaw	= this.net_yaw;
	this.roll	= this.net_roll;
}

//Draws the entity
Entity.prototype.draw = function(gl)
{
	if(this.type == PLAYER_ENTITY)
	{
	}
	else if(this.type == MONSTER_ENTITY)
	{
	}
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

//Creates an entity
EntityDB.create_entity = function(entity_id, coords, packet)
{
	if(entity_id in EntityDB.index)
	{
		alert("Double initialized entity?");
	}
	
	//Create initial entity
	var len = { val : 0 };
	var ent = new Entity(coords, packet, len);
	EntityDB.index[entity_id] = ent;

	if(entity_id == Player.entity_id)
	{
		Player.set_entity(ent);
	}
	
	return len.val;
}

//Update an entity using the given network packet
EntityDB.update_entity = function(entity_id, coords, packet)
{
	var ent = EntityDB.index[entity_id];
	if(!ent)
	{
		//TODO: Should send a forget packet to the server
		return -1;
	}
	return ent.update(coords, packet);
}

//Destroy an entity
EntityDB.destroy_entity = function(entity_id)
{
	if(entity_id == Player.entity_id)
	{
		App.crash("PLAYER DESTROYED");
		return;
	}

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
