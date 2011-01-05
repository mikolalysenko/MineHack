Game = {}

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

	width = canvas.width;
	height = canvas.height;
	
	gl.viewport(0, 0, width, height);
	gl.clearColor(0.0, 0.0, 0.0, 1.0);
	gl.clear(gl.COLOR_BUFFER_BIT |gl.DEPTH_BUFFER_BIT);
	gl.flush();
	
	return 'Ok';
}
