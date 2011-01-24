/*jslint strict: true, undef: true, onevar: true, evil: true, es5: true, adsafe: true, regexp: true, maxerr: 50, indent: 4 */
"use strict";

var Player =
{
	//Entity ID
	entity_id : "",
	
	//Player entity
	entity : null,

	//Units / tick walk speed
	speed : 0.1,

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
	
	//If set, player is chatting
	in_chat : false,
	
	//If set, then player is digging
	digging : false,
	dig_target : [0, 0, 0]
};


Player.init = function()
{
	document.onkeyup = function(event)
	{
		var ev = Player.keys[event.keyCode];
		if(ev)
		{
			Player.input[ev] = 0;
		}
	};
	
	document.onkeydown = function(event)
	{
		var ev = Player.keys[event.keyCode];
		if(ev)
		{
			Player.input[ev] = 1;
		}
	};
	
	Game.canvas.onmousemove = function(event)
	{
		var cx = Game.canvas.width / 2,
			cy = Game.canvas.height / 2;
		
		Player.dx = (event.x - cx) / Game.canvas.width;
		Player.dy = (event.y - cy) / Game.canvas.height;
	};
	
	Game.canvas.onmousedown = function(event)
	{
		Player.input["dig"] = 1;
	}
	
	Game.canvas.onmouseup = function(event)
	{
		Player.input["dig"] = 0;
	}
	
	return "Ok";
}

Player.set_entity_id = function(str)
{
	var res = "";
	for(var i=14; i>=0; i-=2)
	{
		var x = parseInt(str.slice(i, i+2), 16);
		res += String.fromCharCode(x + 0xB0);
	}
	Player.entity_id = res;
}

Player.set_entity = function(ent)
{
	Player.entity = ent;
}

Player.show_chat_input = function()
{
	var chatBox = document.getElementById("chatBox");
	
	if(chatBox.style.display == "none")
	{
		Player.in_chat = true;
	
		chatBox.onkeypress = function(cc)
		{
		
			if(cc.keyCode != 13)
				return true;
		
			var txt = chatBox.value;
			chatBox.value = "";
			chatBox.style.display = "none";
		
			if(txt.length > 128)
			{
				txt = txt.substring(0, 128);
			}
			if(txt.length > 0)
			{
				InputHandler.push_event(["Chat", txt]);
			}
			
			Game.canvas.focus();
			
			Player.in_chat = false;
			
			return false;
		};
	
		chatBox.style.display = "block";	
		chatBox.focus();
	}
}

Player.tick = function()
{
	if(Player.in_chat)
	{
		for(i in Player.input)
		{
			Player.input[i] = 0;
		}
		
		return;
	}
	
	var front = [ -Math.sin(Player.entity.yaw), 0, -Math.cos(Player.entity.yaw) ];
	var right = [ -front[2], 0, front[0]];
	var up = [0, 1, 0];

	var move = function(v, s)
	{
		Player.entity.x += v[0] * s;
		Player.entity.y += v[1] * s;
		Player.entity.z += v[2] * s;
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

	if(Player.input["dig"] == 1)
	{
		var R = Player.eye_ray();
		
		var hit_rec = Map.trace_ray(
			R[0][0], R[0][1], R[0][2], 
			R[1][0], R[1][1], R[1][2],
			5);
			
		if(hit_rec.length > 0)
		{
			var hit = [ Math.floor(hit_rec[0]), 
						Math.floor(hit_rec[1]), 
						Math.floor(hit_rec[2]) ];
		
			if(!Player.digging)
			{
				InputHandler.push_event(["DigStart", hit ]);
				Player.digging = true;
				Player.dig_target = hit;
			}
			else if(
				Player.dig_target[0] != hit[0] ||
				Player.dig_target[1] != hit[1] ||
				Player.dig_target[2] != hit[2] )
			{
				InputHandler.push_event(["DigStop"]);
				InputHandler.push_event(["DigStart", hit ]);
				Player.dig_target = hit;
			}
		}
	}
	else if(Player.digging)
	{
		InputHandler.push_event(["DigStop"]);
		Player.digging = false;
	}


	if(Player.input["chat"] == 1)
	{
		Player.show_chat_input();
		Player.input["chat"] = 0;
	}

	Player.entity.yaw -= Player.dx * Player.dx * Player.dx;

	if(Player.entity.yaw > Math.PI)
		Player.entity.yaw -= 2.0 * Math.PI;
	if(Player.entity.yaw < -Math.PI)
		Player.entity.yaw += 2.0 * Math.PI;
	
	Player.entity.pitch += Player.dy * Player.dy * Player.dy;

	if(Player.entity.pitch < -Math.PI/2.0)
		Player.entity.pitch = -Math.PI/2.0;
	if(Player.entity.pitch > Math.PI/2.0)
		Player.entity.pitch = Math.PI/2.0;

	//Check for dig conditions
	if( Player.digging )
	{
		//Check if we walked away
	 	if( Math.abs(Player.entity.x - Player.dig_target[0]) > DIG_RADIUS ||
			Math.abs(Player.entity.y - Player.dig_target[1]) > DIG_RADIUS ||
			Math.abs(Player.entity.z - Player.dig_target[2]) > DIG_RADIUS )
		{
			alert("walked away");
			InputHandler.push_event(["DigStop"]);
			Player.digging = false;
		}
		//Other possibility, block is gone
		else if(Map.get_block(Player.dig_target[0], Player.dig_target[1], Player.dig_target[2]) == 0)
		{
			InputHandler.push_event(["DigStop"]);
			Player.digging = false;
		}
	}
}


//Returns the player's chunk
Player.chunk = function()
{
	return [
		Math.floor(Player.entity.x) >> CHUNK_X_S,
		Math.floor(Player.entity.y) >> CHUNK_Y_S,
		Math.floor(Player.entity.z) >> CHUNK_Z_S ];
}

Player.eye_ray = function()
{
	var view_m = Player.entity.pose_matrix();
	var d = [ -view_m[2], -view_m[6], -view_m[10] ];
	return [ [Player.entity.x, Player.entity.y, Player.entity.z], d ];
}



