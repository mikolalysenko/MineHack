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
	
	//Camera parameters
	znear : 1.0,
	zfar  : 512.0,
	fov   : Math.PI / 4.0,
		
	//Our local event loops
	tick_interval : null,
	draw_interval : null,
	shadow_interval : null,
	net_update_interval : null,
	
	//Update worker
	update_worker : null,
	
	//Used for debugging
	show_shadows : false,
	
	//Starts preloading the game
	preload : function()
	{
		Game.canvas = document.getElementById("gameCanvas");
		var gl;
		try
		{
			gl = Game.canvas.getContext("experimental-webgl");
		}
		catch(e)
		{
			App.crash('Browser does not support WebGL');
			return false;
		}
	
		if(!gl)
		{
			App.crash('Invalid WebGL object');
			return false;
		}
	
		//Get extensions
		Game.EXT_FPTex = gl.getExtension("OES_texture_float");
		Game.EXT_StdDeriv = gl.getExtension("OES_standard_derivatives");
		Game.EXT_VertexArray = gl.getExtension("OES_vertex_array_object");	
	
		if(!Game.EXT_FPTex)
		{
			App.crash("WebGL implementation does not support floating point textures");
			return false;
		}
	
		Game.gl = gl;
		
		//Start preloading the map
		Map.preload();
		
		//Start the update worker
		Game.update_worker = new Worker("update_worker.js");
		Game.update_worker.onmessage = function(ev)
		{
			switch(ev.data.type)
			{
				case EV_PRINT:
					console.log(ev.data.str);				
				break;

				case EV_RECV:
					recvProtoBuf(raw_to_pbuf(ev.data.raw));
				break;
			
				case EV_CRASH:
					//alert("Update thread crashed");
					App.crash("Update socket error");
				break;
			}
		};
		Game.update_worker.postMessage({
			type: EV_START, 
			lsw: Session.session_id.lsw, 
			msw: Session.session_id.msw});
		
		return true;
	},
	
	//Start the actual game, initializes graphics stuff
	init : function()
	{
		Game.local_ticks = 0;
		Game.crashed = false;
		Game.ready = false;

		var res = Debug.init();
		if(res != "Ok")
		{
			App.crash(res);
			return;
		}
			
		res = Shadows.init();
		if(res != "Ok")
		{
			App.crash(res);
			return;
		}
	
		res = Map.init();
		if(res != "Ok")
		{
			App.crash(res);
			return;
		}
	
		document.getElementById('gamePane').style.display = 'block';
	
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
		Game.tick_interval = setInterval(Game.tick, GAME_TICK_RATE);
		Game.draw_interval = setInterval(Game.draw, GAME_DRAW_RATE);
		Game.shadow_interval = setInterval(Game.update_shadows, GAME_SHADOW_RATE);
		Game.net_update_interval = setInterval(Game.net_update, GAME_NET_UPDATE_RATE);
		
		//Set player input handlers
		Player.init();
	},

	//Stop all intervals
	shutdown : function()
	{
		document.getElementById('gamePane').style.display = 'none';
	
		Game.running = false;
		if(Game.tick_interval)		clearInterval(Game.tick_interval);
		if(Game.draw_interval)		clearInterval(Game.draw_interval);
		if(Game.shadow_interval)	clearInterval(Game.shadow_interval);
		if(Game.net_update_interval)clearInterval(Game.net_update_interval);
		
		window.onresize = null;
		
		Game.update_worker.terminate();
		
		Map.shutdown();
		Debug.shutdown();
		Player.shutdown();
	},

	resize : function()
	{
		Game.canvas.width = window.innerWidth;
		Game.canvas.height = window.innerHeight;
	
		Game.width = Game.canvas.width;
		Game.height = Game.canvas.height;
	
		//Set the dimensions for the UI stuff
		var appPanel = document.getElementById("gamePane");
		appPanel.width = Game.canvas.width;
		appPanel.height = Game.canvas.height;
	},
	
	
	sendProtoBuf : function(pbuf)
	{
		Game.update_worker.postMessage({type:EV_SEND, raw:pbuf_to_raw(pbuf)});
	},
	
	recvProtoBuf : function(pbuf)
	{
		//FIXME: Process protocol buffer from server here
		
		if(pbuf.chat_message)
		{
			document.getElementById("chatLog").innerHTML += pbuf.chat_message;
		}
	},
	
	
	proj_matrix : function(w, h, fov, zfar, znear)
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
	
	},

	//Returns the camera matrix for the game
	camera_matrix : function(width, height, fov, zfar, znear)
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
	},

	//Tick the game
	tick : function()
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
	
		//Update player input
		Player.tick();
		
	},
	
	net_update : function()
	{
		//Send input packet to server				
		var pbuf = new Network.ClientPacket,
			pos = Player.position(),
			orient = Player.orientation();
		
		pbuf.player_update = new Network.PlayerUpdate;
		
		pbuf.player_update.x = pos[0];
		pbuf.player_update.y = pos[1];
		pbuf.player_update.z = pos[2];
		pbuf.player_update.pitch = orient[0];
		pbuf.player_update.yaw = orient[1];
		pbuf.player_update.roll = orient[2];
		
		Game.sendProtoBuf(pbuf);
	},

	//Draw the game
	draw : function()
	{
		var gl = Game.gl;

		//Wait for any asynchronous rendering commands to complete (ie shadow map updates)
		
		gl.viewport(0, 0, Game.width, Game.height);
		gl.clear(gl.DEPTH_BUFFER_BIT);
	
		Sky.draw_bg();

		gl.enable(gl.DEPTH_TEST);

		gl.frontFace(gl.CW);
		gl.cullFace(gl.BACK);
		gl.enable(gl.CULL_FACE);

		Map.draw();
		
		if(Game.show_shadows)
			Shadows.shadow_maps[0].draw_debug();
		
		gl.flush();
		gl.finish();
	},
	
	//Update the shadow maps
	update_shadows : function()
	{
		for(var i=0; i<Shadows.shadow_maps.length; ++i)
		{
			Shadows.shadow_maps[i].begin();
			Map.draw_shadows(Shadows.shadow_maps[i]);	
			Shadows.shadow_maps[i].end();
		}
		Game.gl.flush();
		Game.gl.finish();
	}
};

