"use strict";

var Game = 
{
	//State variables
	crashed : false,
	running : false,
	ready : false,
	
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
	
	
	znear : 1.0,
	zfar  : 512.0,
	fov   : Math.PI / 4.0,
	
	wait_for_heartbeat : false,
	heartbeat_interval : null,
	
	tick_interval : null,
	draw_interval : null,
	shadow_interval : null,
	
	//Used for debugging
	show_shadows : false,
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
	Game.local_ticks = 0;
	Game.crashed = false;
	Game.ready = false;

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
	
	//Get extensions
	Game.EXT_FPTex = gl.getExtension("OES_texture_float");
	Game.EXT_StdDeriv = gl.getExtension("OES_standard_derivatives");
	Game.EXT_VertexArray = gl.getExtension("OES_vertex_array_object");	
	
	if(!Game.EXT_FPTex)
	{
		return "WebGL implementation does not support floating point textures";
	}
	
	
	Game.gl = gl;

	Game.width = canvas.width;
	Game.height = canvas.height;
	
	var res = Debug.init();
	if(res != "Ok")
	{
		return res;
	}
	
	res = Shadows.init(Game.gl);
	if(res != "Ok")
	{
		return res;
	}
	
	//Initialize the map
	res = Map.init(gl);
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
	Game.heartbeat_interval = setInterval(Game.heartbeat, Game.heartbeat_rate);
	
	return 'Ok';
}

//Create projection matrix
Game.proj_matrix = function(w, h, fov, zfar, znear)
{
	var aspect = w / h;
	
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
	
	return [X, 0, 0, 0,
			 0, Y, 0, 0,
		 	 A, B, C, -1,
			 0, 0, D, 0];
	
}

//Creates the total cmera matrix
Game.camera_matrix = function(width, height, fov, zfar, znear)
{
	if(!width)
	{
		width = Game.width;
		height = Game.height;
		fov = Game.fov;
	}
	
	if(!zfar)
	{
		zfar = Game.zfar;
		znear = Game.znear;
	}
	
	return mmult(Game.proj_matrix(width, height, fov, zfar, znear), Player.view_matrix());
}


//Stop all intervals
Game.shutdown = function()
{
	Game.running = false;
	if(Game.heartbeat_interval) clearInterval(Game.heartbeat_interval);
	if(Game.tick_interval)		clearInterval(Game.tick_interval);
	if(Game.draw_interval)		clearInterval(Game.draw_interval);
	if(Game.shadow_interval)_	clearInterval(Game.shadow_interval);
	Map.shutdown();
}

//Handles a heartbeat
Game.handle_heartbeat = function(arr)
{
	if(!Game.ready)
	{
		//Set intervals
		Game.tick_interval = setInterval(Game.tick, GAME_TICK_RATE);
		Game.draw_interval = setInterval(Game.draw, GAME_DRAW_RATE);
		Game.shadow_interval = setInterval(Game.update_shadows, GAME_SHADOW_RATE);
		
		Map.init_worker();
		Game.ready = true;
	}
	else
	{
		//Handle heartbeat as usual
	}
}

//Serialize the game input
Game.serialize_input = function()
{
	BlobBuilder bb;
	return bb.getBlob("application/octet-stream");
}

//Polls the server for events
Game.heartbeat = function()
{
	if(Game.wait_for_heartbeat)
		return;

	Game.wait_for_heartbeat = true;
	
	//Sends a binary message to the server
	asyncGetBinary("/h?k="+Session.session_id, 
		Game.handle_heartbeat(), 
		function() { alert("heartbeat failed"); },
		Game.serialize_input());
}

//Tick the game
Game.tick = function()
{
	//Update network clock/local clock
	++Game.local_ticks;
	
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
	
	//Update player
	Player.tick();
	
	//Update all entities
}



//Draw the game
Game.draw = function()
{
	var gl = Game.gl,
		cam = Game.camera_matrix();
		
	gl.viewport(0, 0, Game.width, Game.height);
	gl.clear(gl.DEPTH_BUFFER_BIT);
	
	if(Game.wait_for_initial_chunks)
	{
		//Draw loading stuff
	}
	else
	{
		Sky.draw_bg();
	
		gl.enable(gl.DEPTH_TEST);
	
		gl.frontFace(gl.CW);
		gl.cullFace(gl.BACK);
		gl.enable(gl.CULL_FACE);

		//Draw map
		Map.draw(gl);
	
		//Draw entities
		EntityDB.draw(gl, cam);
		
		if(Game.show_shadows)
			Shadows.shadow_maps[0].draw_debug();
	}
	
	gl.flush();
}

Game.update_shadows = function()
{
	var gl = Game.gl, i;

	for(i=0; i<Shadows.shadow_maps.length; ++i)
	{
		Shadows.shadow_maps[i].begin(gl);
		Map.draw_shadows(gl, Shadows.shadow_maps[i]);	
		Shadows.shadow_maps[i].end(gl);
	}
}
