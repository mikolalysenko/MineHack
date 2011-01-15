var InputHandler = 
{
	//Member variables go here
	events : []
};

//Event serializers go here
InputHandler.serializers =
{
	"DigBlock" : function(ev)
	{
		var res = new Uint8Array(13);
		var k = 0;
		
		res[k++] = 2;	//Event type code
		
		for(var j=0; j<3; j++)
		{
			var x = Math.round(ev[j+1]);
			
			res[k++] = 	x 		 & 0xff;
			res[k++] = (x >> 8)  & 0xff;
			res[k++] = (x >> 16) & 0xff;
			res[k++] = (x >> 24) & 0xff;
		}
		
		return res.buffer;
	},
	
	"PlaceBlock" : function(ev)
	{
		var res = new Uint8Array(1);
		
		res[0] = 3;				//Event type
		
		//TODO: Pack more data here
		
		
		return res.buffer;
	},
	
	"Chat" : function(ev)
	{
		var utf8str = Utf8.decode(ev[1]);
		
		if(utf8str.length > 128)
			break;
		
		var res = new Uint8Array(utf8str.length+2);
		
		res[0] = 4;					//Event type
		res[1] = utf8str.length;	//String length
		
		for(var i=0; i<utf8str.length; i++)
			res[i+2] = utf8str.charCodeAt(i);
			
		return res.buffer;
	}
};

InputHandler.push_event = function(ev)
{
	//Bandwidth optimization, avoid sending redundant input events
	for(var i=0; i<InputHandler.events.length; i++)
	{
		if(InputHandler.events[i].length != ev.length)
			continue;
	
		var eq = true;
		for(var j=0; j<InputHandler.events[i].length; j++)
		{
			eq &= InputHandler.events[i][j] == ev[j];
			if(!eq)
				break;
		}
		if(eq)
			return;
	}
	InputHandler.events.push(ev);
}


InputHandler.serialize = function()
{
	//Build a binary packet for the event
	var bb = new BlobBuilder();
	
	//Add session id
	bb.append(Session.get_session_id_arr().buffer);
	
	//Add player information
	//bb.append(Player.net_state().buffer);
	
	//Convert events to blob data
	for(var i=0; i<InputHandler.events.length; i++)
	{
		var ev = InputHandler.events[i];
		var serializer = InputHandler.serializers[ev[0]];
		
		if(serializer)
		{
			bb.append(serializer(ev));	
		}
		else
		{
			alert("Unkown input event type");
		}
	}
	
	//Clear out input event queue
	InputHandler.events = [];

	return bb.getBlob("application/octet-stream");		
}
