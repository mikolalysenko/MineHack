var Shadows = {}

Shadows.init = function(gl)
{
	var res = getProgram(gl, "shaders/shadow.fs", "shaders/shadow.vs");
	if(res[0] != "Ok")
	{
		return res[1];
	}
	
	Shadows.shadow_fs 		= res[1];
	Shadows.shadow_vs		= res[2];
	Shadows.shadow_shader	= res[3];
	
	Shadows.shadow_shader.pos_attr = gl.getAttribLocation(Shadows.shadow_shader, "pos");
	if(Shadows.shadow_shader.pos_attr == null)
		return "Could not locate position attribute";

	Shadows.shadow_shader.proj_mat = gl.getUniformLocation(Shadows.shadow_shader, "proj");
	if(Shadows.shadow_shader.proj_mat == null)
		return "Could not locate projection matrix uniform";
	
	Shadows.shadow_shader.view_mat = gl.getUniformLocation(Shadows.shadow_shader, "view");
	if(Shadows.shadow_shader.view_mat == null)
		return "Could not locate view matrix uniform";

	
	Shadows.shadow_maps = [ new ShadowMap(gl, 512, 512, 1, 32) ];
	
	
	//Create quad stuff
	var verts = new Float32Array([
		-1, -1, 0, 1,
		-1, 1, 0, 1,
		1, 1, 0, 1,
		1, -1, 0, 1]);
	
	var ind = new Uint16Array([0, 1, 2, 0, 2, 3]);
	
	Shadows.quad_vb = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, Shadows.quad_vb);
	gl.bufferData(gl.ARRAY_BUFFER, verts, gl.STATIC_DRAW);
	gl.bindBuffer(gl.ARRAY_BUFFER, null);
	
	Shadows.quad_ib = gl.createBuffer();
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, Shadows.quad_ib);
	gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, ind, gl.STATIC_DRAW);
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, null);
	
	res = getProgram(gl, "shaders/shadow_init.fs", "shaders/shadow_init.vs");
	
	if(res[0] != "Ok")
		return res[1];
	
	Shadows.shadow_init_shader	= res[3];
	
	Shadows.shadow_init_shader.pos_attr = gl.getAttribLocation(Shadows.shadow_init_shader, "pos");
	if(Shadows.shadow_init_shader.pos_attr == null)
		return "Could not locate position attribute for shadow init shader";
	
	
	return "Ok";
}

Shadows.init_map = function()
{
	var gl = Game.gl;

	gl.useProgram(Shadows.shadow_init_shader);
	gl.enableVertexAttribArray(Shadows.shadow_init_shader.pos_attr);
	
	gl.disable(gl.BLEND);
	gl.disable(gl.DEPTH_TEST);
	gl.disable(gl.TEXTURE_2D);
	
	gl.bindBuffer(gl.ARRAY_BUFFER, Shadows.quad_vb);
	gl.vertexAttribPointer(Shadows.shadow_init_shader.pos_attr, 4, gl.FLOAT, false, 0, 0);
	
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, Shadows.quad_ib);
	gl.drawElements(gl.TRIANGLES, 6, gl.UNSIGNED_SHORT, 0);
}

//Retrieves a shadow map for a given level of detail
Shadows.get_shadow_map = function()
{

	return Shadows.shadow_maps[0];
}


//A shadow map
var ShadowMap = function(gl, width, height, z_near, z_far)
{
	this.width		= width;
	this.height		= height;
	this.z_near		= z_near;
	this.z_far		= z_far;
	
	this.light_matrix = new Float32Array([ 1, 0, 0, 0,
										 0, 1, 0, 0,
										 0, 0, 1, 0,
										 0, 0, 0, 1 ]);

	this.shadow_tex = gl.createTexture();
	gl.bindTexture(gl.TEXTURE_2D, this.shadow_tex);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
	gl.texParameteri(gl.TEXTURE_2D,	gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
	gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height, 0, gl.RGBA, gl.FLOAT, null);
	gl.bindTexture(gl.TEXTURE_2D, null);
	
	this.depth_rb = gl.createRenderbuffer();
	gl.bindRenderbuffer(gl.RENDERBUFFER, this.depth_rb);
	gl.renderbufferStorage(gl.RENDERBUFFER, gl.DEPTH_COMPONENT16, width, height);
	gl.bindRenderbuffer(gl.RENDERBUFFER, null);
	
	this.fbo = gl.createFramebuffer();
	gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
	gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.shadow_tex, 0);
	gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.RENDERBUFFER, this.depth_rb);
	
	if(!gl.isFramebuffer(this.fbo))
	{
		alert("Could not create shadow map frame buffer");
	}

	gl.bindFramebuffer(gl.FRAMEBUFFER, null);
}

ShadowMap.prototype.calc_light_matrix = function()
{
	//First generate all bounding points on the frustum
	var pts = [], 
		dx, dy, dz,
		camera = Game.camera_matrix(Game.width, Game.height, Game.fov, this.z_near, this.z_far),
		T = m4inv(camera),
		W = new Float32Array(4), 
		P,
		basis = Sky.get_basis(),
		n = basis[0], u = basis[1], v = basis[2],
		i, j, k, l, s, 

		//Z-coordinate dimensions		
		z, z_max = -256.0, z_min = -256.0, z_scale,
		
		//Dimensions for bounding square in uv plane
		side = 100000.0,
		ax = 0,
		ay = 1,
		cx = 0,
		cy = 0;
	
	//Construct the points for the frustum
	for(dx=-1; dx<=1; dx+=2)
	for(dy=-1; dy<=1; dy+=2)
	for(dz=-1; dz<=1; dz+=2)
	{	
		W[0] = 0.5 * dx;
		W[1] = 0.5 * dy;
		W[2] = 0.5 * dz;
		W[3] = 1.0;
		
		P = hgmult(T, W);
		
		pts.push(new Float32Array([dot(P, u), dot(P, v)]));
		
		z = dot(P, n);
		z_max = Math.max(z_max, z);
	}
	
	//Compute minimal bounding square in uv plane
	for(i=0; i<pts.length; ++i)
	for(j=0; j<i; ++j)
	{
	
		dx = pts[i][0] - pts[j][0];
		dy = pts[i][1] - pts[j][1];
			
		l = Math.sqrt(dx*dx + dy*dy);
		
		if(l < 0.0001)
			continue;
		
		dx /= l;
		dy /= l;
		
		//Compute center of square and side length
		var x_min = 10000.0, x_max = -10000.0,
			y_min = 10000.0, y_max = -10000.0,
			px, py;
		
		for(k=0; k<pts.length; ++k)
		{
			px = dx * pts[k][0] + dy * pts[k][1];
			py = dy * pts[k][0] - dx * pts[k][1];
			
			x_min = Math.min(x_min, px);
			x_max = Math.max(x_max, px);
			y_min = Math.min(y_min, py);
			y_max = Math.max(y_max, py);
		}
		
		s  = 0.5 * Math.max(x_max - x_min, y_max - y_min);
			
		if(s < side)
		{
			side = s;
			ax	 = dx;
			ay	 = dy;
			cx	 = (x_min + x_max) / 2.0;
			cy	 = (y_min + y_max) / 2.0;
		}
	}
	
		
	//Time to build the light matrix!
	dx = ax / side;
	dy = ay / side;
	
	z_max = 256.0;
	
	z_scale = -1.0 / (z_max - z_min);
	
	return new Float32Array([
		dx*u[0]+dy*v[0],	dy*u[0]-dx*v[0],	n[0]*z_scale,	0,
		dx*u[1]+dy*v[1],	dy*u[1]-dx*v[1],	n[1]*z_scale,	0,
		dx*u[2]+dy*v[2],	dy*u[2]-dx*v[2],	n[2]*z_scale,	0,
		-cx/side,			-cy/side,			z_min*z_scale,	1]);
}


ShadowMap.prototype.begin = function(gl)
{

	//Calculate light matrix
	this.light_matrix = this.calc_light_matrix();

	gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
	gl.viewport(0, 0, this.width, this.height);
	

	gl.clear(gl.DEPTH_BUFFER_BIT);
	
	//Draw a full quad to the background
	Shadows.init_map();
	

	gl.useProgram(Shadows.shadow_shader);
	gl.uniformMatrix4fv(Shadows.shadow_shader.proj_mat, false, this.light_matrix);
	
	
	
	gl.disable(gl.BLEND);
	gl.enable(gl.DEPTH_TEST);
	
	gl.enable(gl.CULL_FACE);
	gl.cullFace(gl.FRONT);
	gl.frontFace(gl.CW);
	
	//Set offset
	gl.polygonOffset(0, 0);
	gl.enable(gl.POLYGON_OFFSET_FILL);
	
	gl.enableVertexAttribArray(Shadows.shadow_shader.pos_attr);
	
	//Apply bias
	var i;
	for(i=0; i<4; ++i)
	{
		this.light_matrix[4*i]   *= 0.5;
		this.light_matrix[4*i+1] *= 0.5;
	}
	this.light_matrix[12] += 0.5;
	this.light_matrix[13] += 0.5;
}

ShadowMap.prototype.end = function(gl)
{
	gl.bindFramebuffer(gl.FRAMEBUFFER, null);
	
	gl.disable(gl.POLYGON_OFFSET_FILL);
	gl.polygonOffset(0.0, 0.0);

	
	gl.disableVertexAttribArray(Shadows.shadow_shader.pos_attr);
	
	//Generate mipmap
	/*
	gl.bindTexture(gl.TEXTURE_2D, this.shadow_tex);
	gl.generateMipmap(gl.TEXTURE_2D);
	gl.bindTexture(gl.TEXTURE_2D, null);
	*/
}

ShadowMap.prototype.draw_debug = function(gl)
{
	Debug.draw_tex(this.shadow_tex);
}
