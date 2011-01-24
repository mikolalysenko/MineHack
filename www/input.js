"use strict";

var InputHandler = 
{
	forgotten : [],
	chat_log : ""
};

InputHandler.push_event = function(ev)
{
	if(ev[0] == "Chat")
	{
		InputHandler.chat_log += ev[1];
	}
	else if(ev[0] == "Forget")
	{
		InputHandler.forgotten.push(ev[1]);
	}
	else
	{
		//Not yet implemented
	}
}

//Creates the packet header
InputHandler.packet_header = function()
{
	var res = new Uint8Array(36);
	var ent = Player.entity;
	var i = 0;
	
	//Serialize session id
	var session_id = Session.get_session_id_arr();
	for(var j=0; j<8; ++j)
		res[i++] = session_id[j];
	
	//If the entity isn't initialized, just return the empty header
	if(!ent)
		return res;
	
	//Serialize tick count
	var k = Math.floor(Game.game_ticks);
	var lo = Math.floor(k % 4294967296.0);
	var hi = Math.floor(k / 4294967296.0);

	res[i++] =  lo     &0xff;
	res[i++] = (lo>>8) &0xff;
	res[i++] = (lo>>16)&0xff;
	res[i++] = (lo>>24)&0xff;
	res[i++] =  hi     &0xff;
	res[i++] = (hi>>8) &0xff;
	res[i++] = (hi>>16)&0xff;
	res[i++] = (hi>>24)&0xff;
		
	//X coordinate
	k = Math.round(ent.x * NET_COORD_PRECISION);
	res[i++] =  k     &0xff;
	res[i++] = (k>>8) &0xff;
	res[i++] = (k>>16)&0xff;
	res[i++] = (k>>24)&0xff;
	
	//Y coordinate
	k = Math.round(ent.y * NET_COORD_PRECISION);
	res[i++] =  k     &0xff;
	res[i++] = (k>>8) &0xff;
	res[i++] = (k>>16)&0xff;
	res[i++] = (k>>24)&0xff;
	
	//Z coordinate
	k = Math.round(ent.z * NET_COORD_PRECISION);
	res[i++] =  k     &0xff;
	res[i++] = (k>>8) &0xff;
	res[i++] = (k>>16)&0xff;
	res[i++] = (k>>24)&0xff;
	
	//Pitch, yaw, roll
	res[i++] = (Math.floor(ent.pitch* 255.0 / (2.0 * Math.PI)) + 0x1000) & 0xff;
	res[i++] = (Math.floor(ent.yaw 	* 255.0 / (2.0 * Math.PI)) + 0x1000) & 0xff;
	res[i++] = (Math.floor(ent.roll * 255.0 / (2.0 * Math.PI)) + 0x1000) & 0xff;
	
	//Key state
	res[i++] = 
		(Player.input["forward"]	<< 0) |
		(Player.input["back"] 		<< 1) |
		(Player.input["left"]		<< 2) |
		(Player.input["right"]		<< 3) |
		(Player.input["jump"]		<< 4) |
		(Player.input["crouch"]		<< 5);
		
	//Forget event length
	res[i++] =  InputHandler.forgotten.length 	   & 0xff;
	res[i++] = (InputHandler.forgotten.length >> 8) & 0xff;
	
	//Chat size
	res[i++] =  InputHandler.chat_log.length 	   & 0xff;
	res[i++] = (InputHandler.chat_log.length >> 8)  & 0xff;
	
	
	return res;
}


InputHandler.serialize = function()
{
	//Build a binary packet for the event
	var bb = new BlobBuilder();
	
	//Write the packet header
	bb.append(InputHandler.packet_header().buffer);
	
	//Add all forget events
	var entb = new Uint8Array(8);
	for(var i=0; i<InputHandler.forgotten.length; i++)
	{
		var str = InputHandler.forgotten[i];
		for(var j=0; j<8; j++)
		{
			entb[j] = str.charCodeAt(j) & 0xff;
		}
		bb.append(entb.buffer);
	}
	
	//Add chat data
	var chat_array = new Uint8Array(InputHandler.chat_log.length);
	for(var i=0; i<InputHandler.chat_log.length; i++)
	{
		chat_array[i] = InputHandler.chat_log.charCodeAt(i) & 0xff;
	}
	InputHandler.chat_log = "";
	bb.append(chat_array.buffer);

	//Create blob
	return bb.getBlob("application/octet-stream");		
}

