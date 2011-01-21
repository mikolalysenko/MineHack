var InputHandler = 
{
	forgotten : [],
	chat_log : ""
};

InputHandler.push_event = function(ev)
{
	if(ev[0] == "Chat")
	{
		chat_log += ev[1];
	}
	else if(ev[0] == "Forget")
	{
		forgotten.push(ev[1]);
	}
	else
	{
		//Not yet implemented
	}
}


InputHandler.serialize = function()
{
	//Build a binary packet for the event
	var bb = new BlobBuilder();
	
	//Add session id
	bb.append(Session.get_session_id_arr().buffer);
	
	//Add player information
	bb.append(Player.net_state().buffer);
	
	//TODO: Add all forget events
	
	//Add chat data
	var chat_array = new Uint8Array(InputHandler.chat_log.length);
	for(var i=0; i<InputHandler.chat_log.length; i++)
	{
		chat_array[i] = InputHandler.chat_log.charCodeAt(i) & 0xff;
	}
	chat_log = "";
	bb.append(chat_array.buffer);

	//Create blob
	return bb.getBlob("application/octet-stream");		
}
