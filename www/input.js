"use strict";

var InputHandler = 
{
	forgotten : [],
	chat_log : "",
	actions : [],
	action_length : 0,
	has_events : true
};

InputHandler.push_event = function(ev)
{
	if(ev[0] == "Chat")
	{
		if(InputHandler.chat_log.length + ev[1].length >= (1<<16)-1)
			return;
		
		InputHandler.chat_log += ev[1];
	}
	else if(ev[0] == "Forget")
	{
		if(InputHandler.forgotten.length >= (1<<16)-1)
			return;
		InputHandler.forgotten.push(ev[1]);
	}
	else if(ev[0] == "DigStart")
	{
		if(InputHandler.action_length >= (1<<16)-1)
			return;
			
		InputHandler.has_events = true;
		InputHandler.actions.push([ ACTION_DIG_START, TARGET_BLOCK, Game.game_ticks, ev[1] ]);
		InputHandler.action_length += 1 + 1 + 2 + 3;
	}
	else if(ev[0] == "DigStop")
	{
		if(InputHandler.action_length > (1<<16))
			return;
			
		InputHandler.has_events = true;
		InputHandler.actions.push([ ACTION_DIG_STOP, TARGET_NONE, Game.game_ticks ]);
		InputHandler.action_length += 1 + 1 + 2;
	}
	else
	{
		//Not yet implemented
	}
}

//Creates the packet header
InputHandler.packet_header = function()
{
	var res = new Uint8Array(38);
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
	
	//Action size
	res[i++] = InputHandler.action_length & 0xff;
	res[i++] = (InputHandler.action_length>>8) & 0xff;
	
	
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
			entb[j] = (str.charCodeAt(j) - 0xb0) & 0xff;
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
	
	//Serialize action events (this is a bit more complicated)
	for(var i=0; i<InputHandler.actions.length; i++)
	{
		var action = InputHandler.actions[i];
		
		//Write the header
		var tmp = new Uint8Array(4);
		tmp[0] = action[0];
		tmp[1] = action[1];
		
		var delta_tick = Math.floor(Game.game_ticks - action[2]);
		tmp[2] = delta_tick & 0xff;
		tmp[3] = (delta_tick >> 8) & 0xff;
		
		bb.append(tmp.buffer);
		
		//Write target info
		if(action[1] == TARGET_BLOCK)
		{
			var pos = action[3];
			tmp = new Int8Array(3);
			tmp[0] = pos[0] - Math.floor(Player.entity.x);
			tmp[1] = pos[1] - Math.floor(Player.entity.y);
			tmp[2] = pos[2] - Math.floor(Player.entity.z);
			
			bb.append(tmp.buffer);
		}
		else if(action[2] == TARGET_ENTITY)
		{
			alert("Not implemented");
		}
		else if(action[3] == TARGET_RAY)
		{
			alert("Not implemented");
		}
	}
	
	//Clear old actions
	InputHandler.actions = []
	InputHandler.action_length = 0;
	has_events = false;

	//Create blob
	return bb.getBlob("application/octet-stream");		
}

