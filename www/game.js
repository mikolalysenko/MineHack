Game = 
{
	crashed : false,
	running : false,
	
	tick_count : 0,
	
	TICKS_PER_HEARTBEAT : 2,
	
	update_rate : 40,

	znear : 1.0,
	zfar  : 1000.0,
	fov   : 45.0,
	
	input_events : [],
	
	enable_ao : true
};

Game.resize = function()
{
	Game.canvas.width = window.innerWidth;
	Game.canvas.height = window.innerHeight;
	
	Game.width = Game.canvas.width;
	Game.height = Game.canvas.height;
	
	//Set the dimensions for the UI stuff
	var appPanel = document.getElementById("appElem");
	appPanel.width = Game.canvas.width;
	appPanel.height = Game.canvas.height;
	
	//Set UI position
	var uiPanel = document.getElementById("uiPanel");
	
	
	Game.draw();
}

Game.init = function(canvas)
{
	Game.canvas = canvas;

	var gl;
	try
	{
		gl = canvas.getContext("experimental-webgl");
	}
	catch(e)
	{
		return 'Browser does not support WebGL,';
	}
	
	if(!gl)
	{
		return 'Invalid WebGL object';
	}
	
	Game.gl = gl;

	Game.width = canvas.width;
	Game.height = canvas.height;
	
	//Initialize the map
	var res = Map.init(gl);
	if(res != "Ok")
	{
		return res;
	}
	
	res = Player.init();
	if(res != "Ok")
	{
		return res;
	}
	
	//Update the chunks
	Map.update_cache();
	
	//Initialize screen
	Game.resize();
	
	//Start running the game
	Game.running = true;	
	Game.interval = setInterval(Game.tick, Game.update_rate);
	
	return 'Ok';
}

//Create projection matrix
Game.proj_matrix = function()
{
	var aspect = Game.width / Game.height;
	var znear = Game.znear;
	var zfar = Game.zfar;
	
	var ymax = znear * Math.tan(Game.fov * Math.PI / 360.0);
	var ymin = -ymax;
	var xmin = ymin * aspect;
	var xmax = ymax * aspect;
	
	var X = 2.0 * znear / (xmax - xmin);
	var Y = 2.0 * znear / (ymax - ymin);
	var A = (xmax + xmin) / (xmax - xmin);
	var B = (ymax + ymin) / (ymax - ymin);
	var C = -(zfar + znear) / (zfar - znear);
	var D = -2.0 * zfar*znear / (zfar - znear);

	return new Float32Array([X, 0, A, 0,
							 0, Y, B, 0,
						 	 0, 0, C, D,
							 0, 0, -1, 0]);
}

//Creates the total cmera matrix
Game.camera_matrix = function()
{
	return mmult(Game.proj_matrix(), Player.view_matrix());
}

Game.draw = function()
{
	var gl = Game.gl;
	var cam = Game.camera_matrix();
	
	gl.viewport(0, 0, Game.width, Game.height);
	gl.clearColor(0.4, 0.64, 0.9, 1.0);
	gl.clear(gl.COLOR_BUFFER_BIT |gl.DEPTH_BUFFER_BIT);
	
	gl.enable(gl.DEPTH_TEST);
	gl.disable(gl.CULL_FACE);

	Map.draw(gl, cam);
	
	gl.flush();
}

Game.stop = function()
{
	if(Game.running)
	{
		Game.running = false;
		clearInterval(Game.interval);
	}
}

//Input event serializers
Game.event_serializers = {

	"DigBlock" : function(ev)
	{
		var res = new Uint8Array(10);
		var k = 0;
		
		res[k++] = 2;	//Event type code
		
		for(var j=0; j<3; j++)
		{
			var x = Math.round(ev[j+1]);
			
			res[k++] = 	x 		 & 0xff;
			res[k++] = (x >> 8)  & 0xff;
			res[k++] = (x >> 16) & 0xff;
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

//Update event serializers
Game.update_handlers = {
	
	//Set block event
	1 : function(arr)
	{
		if(arr.length < 10)	//Bad packet, drop
			return -1;
			
		//Parse out event contents
		var i = 0;
		var b = arr[i++];
		var x = arr[i] + (arr[i+1]<<8) + (arr[i+2]<<16);
		i += 3;
		var y = arr[i] + (arr[i+1]<<8) + (arr[i+2]<<16);
		i += 3;
		var z = arr[i] + (arr[i+1]<<8) + (arr[i+2]<<16);
		i += 3;
		
		//Execute update
		Map.set_block(x, y, z, b);
		
		return 10;
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

//Pushes an input event to the server
//Event is sent in a packet with all other input events when the next heartbeat event fires
Game.push_event = function(ev)
{
	//Bandwidth optimization, avoid sending redundant input events
	for(var i=0; i<Game.input_events.length; i++)
	{
		if(Game.input_events[i].length != ev.length)
			continue;
	
		var eq = true;
		for(var j=0; j<Game.input_events[i].length; j++)
		{
			eq &= Game.input_events[i][j] == ev[j];
			if(!eq)
				break;
		}
		if(eq)
			return;
	}
	Game.input_events.push(ev);
}

//Polls the server for events
Game.heartbeat = function()
{
	//Build a binary packet for the event
	var bb = new BlobBuilder();
	
	//Add player information
	bb.append(Player.net_state().buffer);
	
	//Convert events to blob data
	for(var i=0; i<Game.input_events.length; i++)
	{
		var ev = Game.input_events[i];
		var serializer = Game.event_serializers[ev[0]];
		
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
	Game.input_events = [];
		
	asyncGetBinary("/h?k="+Session.session_id, function(arr)
	{
		var i = 0;
		while(i < arr.length)
		{
			var t = arr[i++];
			
			var handler = Game.update_handlers[t];
			
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
				alert("bad update type?  dropping packet");
				return;
			}
		}
	}, bb.getBlob("application/octet-stream"));
}

Game.tick = function()
{
	if(Game.tick_count % Game.TICKS_PER_HEARTBEAT == 0)
		Game.heartbeat();

	++Game.tick_count;

	//Update game state
	Player.tick();

	//Update cache
	Map.update_cache();
	
	//Redraw
	Game.draw();
}
