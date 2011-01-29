"use strict";

Game = 
{
	crashed : false,
	running : false,
	
	//Clock stuff:
	// Each tick is approximately 40ms (this is not very precise due to scheduling issues)
	
	//There are 3 timers in the game
	// Local ticks measures the total number of ticks since the app started.
	//This is used internally for engine related functions (ie ping, event scheduling, etc.)
	local_ticks : 0,
	
	//Game ticks is the approximate local time.  This is the tick counter that
	//should be used for simulation.  It is approximately equal to (net_ticks - ping)
	//Note that this is a floating point number, not an integer quantity!
	game_ticks : 0,	
	
	//This is out guess at what the network clock time is.  Our local simulation
	//always runs somewhat behind this.
	net_ticks : 0,
	
	//This is the amount of time we are behind the network counter (in ticks)
	ping : 0,
	
	TICKS_PER_HEARTBEAT : 20,
	FINE_TICKS_PER_HEARTBEAT : 2,
	
	update_rate : 40,

	znear : 1.0,
	zfar  : 256.0,
	fov   : Math.PI / 2.0,
	
	wait_for_heartbeat : false,
	
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
Game.proj_matrix = function(w, h, fov)
{
	var aspect = w / h;
	var znear = Game.znear;
	var zfar = Game.zfar;
	
	var ymax = znear * Math.tan(0.5 * fov);
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
Game.camera_matrix = function(width, height, fov)
{
	if(!width)
	{
		width = Game.width;
		height = Game.height;
		fov = Game.fov;
	}

	return mmult(Game.proj_matrix(width, height, fov), Player.entity.pose_matrix());
}

Game.draw = function()
{
	var gl = Game.gl;
	var cam = Game.camera_matrix();
	
	gl.viewport(0, 0, Game.width, Game.height);
	gl.clearColor(0.4, 0.64, 0.9, 1.0);
	gl.clear(gl.COLOR_BUFFER_BIT |gl.DEPTH_BUFFER_BIT);
	
	gl.enable(gl.DEPTH_TEST);
	
	gl.frontFace(gl.CW);
	gl.enable(gl.CULL_FACE);

	//Draw map
	Map.draw(gl, cam);
	
	//Draw entities
	EntityDB.draw(gl, cam);
	
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
	Game.wait_for_heartbeat = true;
	
	//Sends a binary message to the server
	asyncGetBinary("/h?k="+Session.session_id, 
		UpdateHandler.handle_update_packet, 
		function() { alert("error"); },
		InputHandler.serialize());
}

Game.tick = function()
{
	if(	!Game.wait_for_heartbeat && 
		(Game.local_ticks % Game.TICKS_PER_HEARTBEAT == 0 ||
		(InputHandler.has_events && (Game.local_ticks % Game.FINE_TICKS_PER_HEARTBEAT == 0)) ) )
		Game.heartbeat();

	//Update network clock/local clock
	++Game.local_ticks;

	//Wait for player entity packet before starting game
	if(!Player.entity)
		return;
	
	//Goal: Try to interpolate local clock so that  = remote_clock - ping 
	if(Game.game_ticks < Game.net_ticks - 2.0 * Game.ping)
	{
		Game.game_ticks = Math.floor(0.5 * (Game.net_ticks - Game.ping) + 0.5 * Game.game_ticks);
	}
	else if(Game.game_ticks >= Math.floor(Game.net_ticks - Game.ping))
	{
		//Whoops!  Too far ahead, do nothing for a few frames to catch up
	}
	else
	{	
		++Game.game_ticks;
	}
	
		
	//Tick entities
	EntityDB.tick();

	//Update game state
	Player.tick();

	//Update cache
	Map.update_cache();
	
	//Redraw
	Game.draw();
}
