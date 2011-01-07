Game = 
{
	crashed : false,
	running : false,

	znear : 1.0,
	zfar  : 1000.0,
	fov   : 45.0,
	
	cam_pos   : [16.0, 16.0, 40.0],
	cam_pitch : 0.0,
	cam_yaw   : 0.0,
	
	enable_ao : true
};

Game.resize = function()
{
	Game.canvas.width = window.innerWidth;
	Game.canvas.height = window.innerHeight;
	
	Game.width = Game.canvas.width;
	Game.height = Game.canvas.height;
	
	Game.draw();
}

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
	if(res != "Ok")
	{
		return res;
	}
	
	//TESTING CODE:  create a random chunk and stick it in the map
	var data = new Array(34*34*34);
	
	for(var i=0; i<data.length; i++)
	{
		data[i] = 0;
		
		if(Math.random() < 0.01)
		{
			data[i] = 3;
		}
	}	
	
	//Add the chunk to the map
	Map.add_chunk(new Chunk(0, 0, 0, data));
	
	Game.running = true;
	
	Game.resize();
	
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
	
	var X = 2.0 * znear / (xmax - xmin);
	var Y = 2.0 * znear / (ymax - ymin);
	var A = (xmax + xmin) / (xmax - xmin);
	var B = (ymax + ymin) / (ymax - ymin);
	var C = -(zfar + znear) / (zfar - znear);
	var D = -2.0 * zfar*znear / (zfar - znear);

	return new Float32Array([X, 0, A, 0,
							 0, Y, B, 0,
						 	 0, 0, C, D,
							 0, 0, -1, 0]);
}

//Create view matrix
Game.view_matrix = function()
{
	var cp = Math.cos(Game.cam_pitch * Math.PI / 180.0);
	var sp = Math.sqrt(1.0 - cp*cp);
	var cy = Math.cos(Game.cam_yaw * Math.PI / 180.0);
	var sy = Math.sqrt(1.0 - cy*cy);
	
	return new Float32Array([
		 cy*cp,  sy,  cy*sp, 0,
		-sy*cp,  cy, -sy*sp, 0,
		-sp,     0,   cp,    0,
		-Game.cam_pos[0], -Game.cam_pos[1], -Game.cam_pos[2], 1]);
}

//Creates the total cmera matrix
Game.camera_matrix = function()
{
	return mmult(Game.proj_matrix(), Game.view_matrix());
}

Game.draw = function()
{
	var gl = Game.gl;
	var cam = Game.camera_matrix();
	
	gl.viewport(0, 0, Game.width, Game.height);
	gl.clearColor(0.0, 0.0, 0.0, 1.0);
	gl.clear(gl.COLOR_BUFFER_BIT |gl.DEPTH_BUFFER_BIT);
	
	gl.enable(gl.DEPTH_TEST);
	gl.disable(gl.CULL_FACE);

	Map.draw(gl, cam);
	
	gl.flush();
}
