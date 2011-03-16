"use strict";

var Player =
{
	//Units / tick walk speed
	speed : 0.4,

	//Default keycodes (these can be reconfigured)
	keys : {
		87 : "forward",
		83 : "backward",
		65 : "left",
		68 : "right",
		32 : "jump",
		67 : "crouch",
		66 : "dig",
		84 : "chat"
	},
	
	//Input state
	input : {
		"forward" : 0,
		"backward" : 0,
		"left" : 0,
		"right" : 0,
		"jump" : 0,
		"crouch" : 0,
		"dig" : 0,
		"chat" : 0
	},
	
	//Mouse delta
	dx : 0,
	dy : 0,
	
	//Temporary storage for position variables
	x : (1<<19),
	y : (1<<19),
	z : (1<<19),
	pitch : 0,
	yaw : 0,
	roll : 0,
	
	//If set, player is chatting
	in_chat : false,

	init : function()
	{
		document.onkeyup = function(event)
		{
			if(Player.in_chat)
				return true;
		
			var ev = Player.keys[event.keyCode];
			if(ev)
			{
				Player.input[ev] = 0;
			}
			return false;
		};
	
		document.onkeydown = function(event)
		{
			if(Player.in_chat)
				return true;		
		
			var ev = Player.keys[event.keyCode];
			if(ev)
			{
				Player.input[ev] = 1;
			}
			return false;
		};
	
		var body = document.getElementById("docBody");
	
		body.onmousemove = function(event)
		{
			if(Player.in_chat)
				return true;
		
			var cx = Game.canvas.width / 2,
				cy = Game.canvas.height / 2;
		
			Player.dx = (event.x - cx) / Game.canvas.width;
			Player.dy = (event.y - cy) / Game.canvas.height;
			return false;
		};
	
		body.onmousedown = function(event)
		{
			if(Player.in_chat)
				return true;
			Player.input["dig"] = 1;
			return false;
		}
	
		body.onmouseup = function(event)
		{
			if(Player.in_chat)
				return true;
			Player.input["dig"] = 0;
			return false;
		}
	},
	
	shutdown : function()
	{
		document.onkeyup = null;
		document.onkeydown = null;
		
		var body = document.getElementById("docBody");
		body.onmousemove = null;
		body.onmousedown = null;
		body.onmouseup = null;
	},

	show_chat_input : function()
	{
		if(!Player.in_chat)
		{
			var chatBox = document.getElementById("chatBox");
		
			chatBox.onkeypress = function(cc)
			{
				if(cc.keyCode != 13)
					return true;
		
				var txt = chatBox.value;
				chatBox.value = "";
				chatBox.style.display = "none";
				chatBox.onkeypress = null;
		
				if(txt.length > 0)
				{
					var pbuf = new Network.ClientPacket;
					pbuf.chat_message = txt;
					Game.sendProtoBuf(pbuf);
				}
			
				Player.in_chat = false;
				Game.canvas.focus();
			
				return false;
			};
	
			chatBox.style.display = "block";	
			chatBox.focus();
			Player.in_chat = true;
		}
	},

	tick : function()
	{
		if(Player.input["chat"] == 1)
		{
			Player.show_chat_input();
		}

		if(Player.in_chat)
		{
			for(var i in Player.input)
			{
				if(typeof i == "string")
					Player.input[i] = 0;
			}
		
			return;
		}
	
		var orientation = Player.orientation(),
			tpos = Player.position();
		var front = [ -Math.sin(orientation[1]), 0, -Math.cos(orientation[1]) ];
		var right = [ -front[2], 0, front[0]];
		var up = [0, 1, 0];

		var move = function(v, s)
		{
			for(var i=0; i<3; ++i)
			{
				tpos[i] += v[i] * s;
			}
		}

		if(Player.input["forward"] == 1)
			move(front, Player.speed);
	
		if(Player.input["backward"] == 1)
			move(front, -Player.speed);
	
		if(Player.input["left"] == 1)
			move(right, -Player.speed);
	
		if(Player.input["right"] == 1)
			move(right, Player.speed);
		
		if(Player.input["jump"] == 1)
			move(up, Player.speed);
		
		if(Player.input["crouch"] == 1)
			move(up, -Player.speed);

		//Update position
		Player.set_position(tpos);

		//Update heading
		orientation[1] -= Player.dx * Player.dx * Player.dx;

		if(orientation[1] > Math.PI)
			orientation[1] -= 2.0 * Math.PI;
		if(orientation[1] < -Math.PI)
			orientation[1] += 2.0 * Math.PI;
	
		orientation[0] += Player.dy * Player.dy * Player.dy;

		if(orientation[0] < -Math.PI/2.0)
			orientation[0] = -Math.PI/2.0;
		if(orientation[0] > Math.PI/2.0)
			orientation[0] = Math.PI/2.0;

		Player.set_orientation(orientation);

		if(Player.input["dig"] == 1)
		{
			var R = Player.eye_ray();
		
			var hit_rec = Map.trace_ray(
				R[0][0], R[0][1], R[0][2], 
				R[1][0], R[1][1], R[1][2],
				5);
			
			if(hit_rec.length > 0)
			{
				var hit = [ Math.round(hit_rec[0]), 
							Math.round(hit_rec[1]), 
							Math.round(hit_rec[2]) ];

				//FIXME:  This will need to get updated		
			}
		}
	},
	
	//Returns player's orientation
	// Pitch, yaw, roll
	orientation : function()
	{
		return [ Player.pitch, Player.yaw, Player.roll ];
	},

	//Returns player's position
	position : function()
	{
		return [ Player.x, Player.y, Player.z ];
	},

	//Updates the player's orientation 
	set_orientation : function(orient)
	{
		Player.pitch = orient[0];
		Player.yaw	 = orient[1];
		Player.roll	 = orient[2];
	},

	//Updates the player's position
	set_position : function(pos)
	{
		Player.x = pos[0];
		Player.y = pos[1];
		Player.z = pos[2];
	},

	//Returns the player's chunk
	chunk : function()
	{
		var pos = Player.position();

		return [
			Math.floor(pos[0]) >> CHUNK_X_S,
			Math.floor(pos[1]) >> CHUNK_Y_S,
			Math.floor(pos[2]) >> CHUNK_Z_S ];
	},

	//Retrieves the player's eye ray
	eye_ray : function()
	{
		var view_m = Player.view_matrix();
		return [ Player.position(), [ -view_m[2], -view_m[6], -view_m[10] ] ];
	},

	//Retrieves the view matrix for the player
	view_matrix : function()
	{
		var orient	= Player.orientation(),
			pos 	= Player.position(),

			cp = Math.cos(orient[0]),
			sp = Math.sin(orient[0]),
			cy = Math.cos(orient[1]),
			sy = Math.sin(orient[1]),
			cr = Math.cos(orient[2]),
			sr = Math.sin(orient[2]);
	
		var rotp = [
			 1,   0,  0, 0,
			 0,  cp, sp, 0,
			 0, -sp, cp, 0,
			 0,   0,  0, 1]; 
			  
		var roty = [
			 cy, 0, sy, 0,
			  0, 1,  0, 0,
			-sy, 0, cy, 0,
			  0, 0,  0, 1];
			  
		var rotr = [
			 cr, sr, 0, 0,
			-sr, cr, 0, 0,
			  0,  0, 1, 0,
			  0,  0, 0, 1];
	
		var rot = mmult(mmult(rotp, roty), rotr);
	
		//Need to shift by player chunk in order to avoid numerical problems
		var c = Player.chunk();	
		c[0] *= CHUNK_X;
		c[1] *= CHUNK_Y;
		c[2] *= CHUNK_Z;
		
	
		var trans = [
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			c[0]-pos[0], c[1]-pos[1], c[2]-pos[2], 1]
	
		return mmult(rot, trans);
	}
};
