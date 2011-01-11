var Player =
{
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
		66 : "dig"
	},
	
	//Input state
	input : {
		"forward" : 0,
		"backward" : 0,
		"left" : 0,
		"right" : 0,
		"jump" : 0,
		"crouch" : 0,
		"dig" : 0
	},
	
	//
	dx : 0,
	dy : 0,
	
	//Player coordinates
	pos : [(1<<20), (1<<20), (1<<20)],
	pitch : 0,
	yaw : 0	
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

Player.net_state = function()
{
	var res = new Uint8Array(13);
	var i = 0;
	
	res[i++] = 1;	//Player update event id
	
	for(var j=0; j<3; j++)
	{
		var x = Math.round(Player.pos[j]);
		res[i++] =  x     &0xff;
		res[i++] = (x>>8) &0xff;
		res[i++] = (x>>16)&0xff;
	}
	res[i++] = Math.round((180.0 + Player.yaw) * 255.0 / 360.0);
	res[i++] = Math.round(Player.pitch + 45.0);
	res[i++] = 
		(Player.input["forward"]	<< 0) |
		(Player.input["back"] 		<< 1) |
		(Player.input["left"]		<< 2) |
		(Player.input["right"]		<< 3) |
		(Player.input["jump"]		<< 4) |
		(Player.input["crouch"]	<< 5) |
		(Player.input["dig"]		<< 6);
	
	return res;
}

Player.tick = function()
{
	var front = [ -Math.sin(Player.yaw), 0, -Math.cos(Player.yaw) ];
	var right = [ -front[2], 0, front[0]];
	var up = [0, 1, 0];

	var move = function(v, s)
	{
		Player.pos[0] += v[0] * s;
		Player.pos[1] += v[1] * s;
		Player.pos[2] += v[2] * s;
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
			
		if(hit_rec != [])
		{
			Game.push_event(["DigBlock", hit_rec[0], hit_rec[1], hit_rec[2]]);
		}
	}


	Player.yaw -= Player.dx * Player.dx * Player.dx;

	if(Player.yaw > Math.PI)
		Player.yaw -= 2.0 * Math.PI;
	if(Player.yaw < -Math.PI)
		Player.yaw += 2.0 * Math.PI;
	
	Player.pitch += Player.dy * Player.dy * Player.dy;

	if(Player.pitch < -Math.PI/2.0)
		Player.pitch = -Math.PI/2.0;
	if(Player.pitch > Math.PI/2.0)
		Player.pitch = Math.PI/2.0;

}

//Returns the player's chunk
Player.chunk = function()
{
	return [
		Math.floor(Player.pos[0]) >> 5,
		Math.floor(Player.pos[1]) >> 5,
		Math.floor(Player.pos[2]) >> 5 ];
}

//Create view matrix
Player.view_matrix = function()
{
	var cp = Math.cos(Player.pitch);
	var sp = Math.sin(Player.pitch);
	var cy = Math.cos(Player.yaw);
	var sy = Math.sin(Player.yaw);
	
	var rotp = new Float32Array([
		 1,   0,  0, 0,
		 0,  cp, sp, 0,
		 0, -sp, cp, 0,
		 0,   0,  0, 1]); 
		  
	var roty = new Float32Array([
		 cy, 0, sy, 0,
		  0, 1,  0, 0,
		-sy, 0, cy, 0,
		  0, 0,  0, 1]);
	
	var rot = mmult(rotp, roty);
		
	var c = Player.chunk();	
	c[0] *= 32;
	c[1] *= 32;
	c[2] *= 32;
		
	var trans = new Float32Array([
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		c[0]-Player.pos[0], c[1]-Player.pos[1], c[2]-Player.pos[2], 1])
	
	return mmult(rot, trans);
}


Player.eye_ray = function()
{
	var view_m = Player.view_matrix();
	var d = [ -view_m[2], -view_m[6], -view_m[10] ];
	return [ Player.pos, d ];
}



