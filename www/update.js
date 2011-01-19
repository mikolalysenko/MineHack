var UpdateHandler = 
{
	//Add global variables here
};



UpdateHandler.handlers = 
{
	
	//Set block event
	1 : function(arr)
	{
		if(arr.length < 13)	//Bad packet, drop
			return -1;
			
		//Parse out event contents
		var i = 0;
		var b = arr[i++];
		var x = arr[i] + (arr[i+1]<<8) + (arr[i+2]<<16) + (arr[i+3]<<24);
		i += 4;
		var y = arr[i] + (arr[i+1]<<8) + (arr[i+2]<<16) + (arr[i+3]<<24);
		i += 4;
		var z = arr[i] + (arr[i+1]<<8) + (arr[i+2]<<16) + (arr[i+3]<<24);
		i += 4;
		
		//Execute update
		Map.set_block(x, y, z, b);
		
		return 13;
	},
	
	//Chat event
	2 : function(arr)
	{
		if(arr.length < 1)
			return -1;
		var name_len = arr[0];
		
		if(arr.length < name_len+2)
			return -1;
		var name_str_utf = arr.slice(1, name_len+1);
		var msg_len = arr[name_len+1];
		
		var len = name_len + msg_len + 2;
		
		if(arr.length < len)
			return -1;
		var msg_str_utf = arr.slice(name_len+2, name_len+msg_len+2);

		var uint2utf = function(v)
		{
			var res = ""
			for(var i=0; i<v.length; i++)
				res += String.fromCharCode(v[i]);
			
			res = Utf8.decode(res);
			
			return res.replace("&", "&amp;")
					  .replace("<", "&lt;")
					  .replace(">", "&gt;");
		}
		
		var name_str = uint2utf(name_str_utf);
		var msg_str = uint2utf(msg_str_utf);
		
		var html_str = "<b>" + name_str + "</b>: " + msg_str + "<br/>";
		
		//Add to chat log
		var chat_log = document.getElementById("chatLog");
		chat_log.innerHTML += html_str;
		
		return len;
	},
	
	//Update entity
	3 : function(arr)
	{
		//Read out entity id and length
		if(arr.length < 10)
			return -1;
		var entity_id = arr2str(arr.slice(0, 8));
		
		//Read out length of data
		var len = arr[8] + (arr[9] << 8);
		if(arr.length < 10 + len)
			return -1;
		
		EntityDB.update_entity(entity_id, arr.slice(10, 10 + len));
		return 10+len;
	},
	
	//Delete entity
	4 : function(arr)
	{
		//Read out entity id
		if(arr.length < 8)
			return -1;
		var entity_id = arr2str(arr.slice(0, 8));
		EntityDB.destroy_entity(entity_id);
		return 8;
	},
	
	//Clock synchronize event
	5 : function(arr)
	{
		if(arr.length < 8)
			return -1;
			
		Game.tick_lo = arr[0] + (arr[1]<<8) + (arr[2]<<16) + (arr[3]<<24);
		Game.tick_hi = arr[4] + (arr[5]<<8) + (arr[6]<<16) + (arr[7]<<24);
		
		return 8;
	}
	
};


//Handles a block update packet
UpdateHandler.handle_update_packet = function(arr)
{
	Game.wait_for_heartbeat = false;

	var i = 0;
	
	while(i < arr.length)
	{
		var t = arr[i++];
		
		var handler = UpdateHandler.handlers[t];
		
		if(handler)
		{
			var l = handler(arr.slice(i));
			if(l < 0)
			{
				alert("dropping packet");
				return;
			}
			i += l;
		}
		else
		{
			alert("bad update type: " + t + " --  dropping packet");
			return;
		}
	}
}

