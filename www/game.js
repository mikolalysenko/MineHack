Game = 
{
	znear : 0.01,
	zfar  : 1000.0,
	fov   : 45.0
};

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

	Game.width = canvas.width;
	Game.height = canvas.height;
	
	//Initialize the map
	var res = Map.init(gl);
	if(res != 'Ok')
	{
		return res;
	}
	
	//TESTING CODE:  create a random chunk and stick it in the map
	var data = new Array(34*34*34);
	for(var i=0; i<data.length; i++)
	{
		data[i] = 0;
		if(Math.random() <= 0.04)
			data[i] = 1;
	}
	
	//Add the chunk to the map
	Map.add_chunk(new Chunk(0, 0, 0, data));
	
	return 'Ok';
}

//Create projection matrix
Game.proj_matrix = function()
{
	var aspect = Game.width / Game.height;
	var znear = Game.znear;
	var zfar = Game.zfar;
	var ymax = znear * Math.tan(Game.fov * Math.PI / 360.0);
	var ymin = -ymax;
	var xmin = ymin * aspect;
	var xmax = ymax * aspect;
	
	var X = 2 * znear / (xmax - xmin);
	var Y = 2 * znear / (ymax - ymin);
	var A = (xmax + xmin) / (xmax - xmin);
	var B = (ymax + ymin) / (ymax - ymin);
	var C = (zfar + znear) / (zfar - znear);
	var D = 2*zfar*znear / (zfar - znear);

	return new Float32Array([X, 0, A, 0,
							 0, Y, B, 0,
						 	 0, 0, C, D,
							 0, 0, -1, 0]);
}

Game.draw = function()
{
	var gl = Game.gl;
	gl.viewport(0, 0, Game.width, Game.height);
	gl.clearColor(0.0, 0.0, 0.0, 1.0);
	gl.clear(gl.COLOR_BUFFER_BIT |gl.DEPTH_BUFFER_BIT);

	Map.draw(gl, Game.proj_matrix);
	
	gl.flush();
}
