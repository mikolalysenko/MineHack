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
	
	//Player coordinates
	pos : [(1<<20), (1<<20) + 64, (1<<20)],
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
	}
	
	document.onkeydown = function(event)
	{
		var ev = Player.keys[event.keyCode];
		if(ev)
		{
			Player.input[ev] = 1;
		}
	}
	
	
	
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
		
}
