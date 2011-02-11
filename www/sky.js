var Sky = 
{
}

Sky.get_shadow_fudge = function()
{
	var dir = Sky.get_sun_dir(),
		angle = dot(dir, [1, 0, 0]);
		
	return 0.25 + 0.5 * Math.abs(angle);
}

//Retrieves the light direction
Sky.get_sun_dir = function()
{
	var time_of_day = (Game.game_ticks % 1000),
		angle = time_of_day * Math.PI / 500.0;
	
	if(time_of_day > 500)
	{
		angle -= Math.PI;
	}

	return [Math.cos(angle), Math.sin(angle), 0];
}

Sky.get_basis = function()
{
	var n = Sky.get_sun_dir(),
		u = [0, 0, 1],
		v = cross(n, u),
		l = Math.sqrt(dot(v,v)),
		i;
		
	for(i=0; i<3; ++i)	
		v[i] /= l;
		
	return [n, u, v];
}


//Returns the color of the sunlight
Sky.get_sun_color = function()
{
	return [0.79, 0.81,  0.85];
}

//Draws the sky background
Sky.draw_bg = function()
{
}
