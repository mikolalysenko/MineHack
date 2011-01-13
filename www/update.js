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
	}
};


//Handles a block update packet
UpdateHandler.handle_update_packet = function(arr)
{
	var i = 0;
	
	while(i < arr.length)
	{
		var t = arr[i++];
		
		var handler = UpdateHandler.handlers[t];
		
		if(handler)
		{
			var l = handler(arr.slice(1, arr.length));
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

