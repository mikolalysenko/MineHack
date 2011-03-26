"use strict";

var Sky = 
{
	day_offset : 0,

	//Ranges from -1 to +1
	// +/-1 = sunset
	//    0 = sunrise
	time_of_day : function()
	{
		//return (((Game.game_ticks + Sky.day_offset) % 8000) / 4000.0) - 1.0;
		return 0.5;
	},

	get_shadow_fudge : function()
	{
		return 0.01;
	},

	//Retrieves the light direction
	// (sun/moon)
	get_sun_dir : function()
	{
		var angle = Sky.time_of_day() * Math.PI;
	
		if(angle < 0)
			angle += Math.PI;
	
		return [Math.cos(angle), Math.sin(angle), 0];
	},

	get_basis : function()
	{
		var n = Sky.get_sun_dir(),
			u = [0, 0, 1],
			v = cross(n, u),
			l = Math.sqrt(dot(v,v)),
			i;
		
		for(i=0; i<3; ++i)	
			v[i] /= l;
		
		return [n, u, v];
	},


	//Returns the color of the sunlight
	get_sun_color : function()
	{
		var t = Sky.time_of_day();
	
		if(t < 0.0)
		{
			//Night
			return [0.23, 0.24,  0.4]
		}
		else
		{
			return [0.79, 0.81,  0.85];
		}
	},

	//Draws the sky background
	draw_bg : function()
	{
		var gl = Game.gl,
			t = Sky.time_of_day();
	
		if(t < 0.0)
		{
			gl.clearColor(0.1, 0.18, 0.28, 1.0);
			gl.clear(gl.COLOR_BUFFER_BIT);		
		}
		else
		{
			gl.clearColor(0.4, 0.64, 0.9, 1.0);
			gl.clear(gl.COLOR_BUFFER_BIT);
		}
	}
}
