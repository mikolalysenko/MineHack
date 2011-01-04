var Render =
{
};

//Initialize the OpenGL contet
Render.init = function(canvas)
{
	Render.canvas = canvas;

	try
	{
		Render.gl = canvas.getContext("experimental-webgl");
	}
	catch(e)
	{
		return 'Browser does not support WebGL';
	}

	width = canvas.width;
	height = canvas.height;
	
	gl = Render.gl;
	gl.viewport(0, 0, width, height);
	
	gl.clearColor(0, 0, 0, 0);
	gl.clear(gl.COLOR_BUFFER_BIT |gl.DEPTH_BUFFER_BIT);
	gl.flush();
	
	
	return 'Ok';
};
