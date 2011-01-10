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
		67 : "crouch"
	},
	
	//Input state
	input : {
		"forward" : 0,
		"backward" : 0,
		"left" : 0,
		"right" : 0,
		"jump" : 0,
		"crouch" : 0
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
	
	return "Ok";
}

Player.tick = function()
{
	front = [ -Math.sin(Player.pitch), 0, -Math.cos(Player.pitch) ];
	right = [ -front[2], 0, -front[0]];
	up = [0, 1, 0];

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


	Player.pitch += Player.dx * Player.dx * Player.dx * 4.0;
	
	if(Player.pitch > 180.0)
		Player.pitch -= 360.0;
	if(Player.pitch < -180.0)
		Player.pitch += 360.0;
	
	Player.yaw += Player.dy * Player.dy * Player.dy * 4.0;
	
	if(Player.yaw < -45.0)
		Player.yaw = -45.0;
	if(Player.yaw > 45.0)
		Player.yaw = 45.0;
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
	var cp = Math.cos(Player.pitch * Math.PI / 180.0);
	var sp = Math.sqrt(1.0 - cp*cp);
	var cy = Math.cos(Player.yaw * Math.PI / 180.0);
	var sy = Math.sqrt(1.0 - cy*cy);
	
	var rot = new Float32Array([
		 cy*cp,  sy,  cy*sp, 0,
		-sy*cp,  cy, -sy*sp, 0,
		-sp,     0,   cp,    0,
		0, 0, 0, 1]);
		
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

