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
	window.onresize = function(event)
	{
		if(Game.running)
		{
			Game.resize();
		}
	}
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

Game.shutdown = function()
{
	if(Game.running)
	{
		Game.running = false;
		clearInterval(Game.interval);
	}
}

//Polls the server for events
Game.heartbeat = function()
{
	//Sends a binary message to the server
	asyncGetBinary("/h?k="+Session.session_id, 
		UpdateHandler.handle_update_packet, 
		function() {},
		InputHandler.serialize());
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
