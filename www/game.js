Game = 
{
	crashed : false,
	running : false,
	
	tick_count : 0,
	
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
		
		if(ev[0] == "DigBlock")
		{
			var res = new Uint8Array(10);
			var k = 0;
			
			res[k++] = 2;						//Event type
			
			for(var j=0; j<3; j++)
			{
				var x = Math.round(ev[j+1]);
				
				res[k++] = 	x 		 & 0xff;
				res[k++] = (x >> 8)  & 0xff;
				res[k++] = (x >> 16) & 0xff;
			}
			
			bb.append(res.buffer);
		}
		else
		{
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
			
			if(t == 1)					//SetBlock event
			{
				if(i + 10 > arr.length)	//Bad packet, drop
				{
					debugger;
					alert("dropping packet");
					return;
				}
			
				var b = arr[i++];
				var x = arr[i] + (arr[i+1]<<8) + (arr[i+2]<<16);
				i += 3;
				var y = arr[i] + (arr[i+1]<<8) + (arr[i+2]<<16);
				i += 3;
				var z = arr[i] + (arr[i+1]<<8) + (arr[i+2]<<16);
				i += 3;
				
				Map.set_block(x, y, z, b);
			}
			else
			{
				//Unknown event type
				alert("Unkown packet type?");
			}
		}
	}, bb.getBlob("application/octet-stream"));
}

Game.tick = function()
{
	if(Game.tick_count % 2 == 0)
		Game.heartbeat();

	++Game.tick_count;

	//Update game state
	Player.tick();

	//Update cache
	Map.update_cache();
	
	//Redraw
	Game.draw();
}
