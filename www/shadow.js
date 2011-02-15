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

	
	Shadows.shadow_maps = [ 
		new ShadowMap(gl, 256, 256, 1, 16),
		new ShadowMap(gl, 256, 256, 16, 64),
		new ShadowMap(gl, 256, 256, 64, 256)
		];
	
	
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
		camera = Game.proj_matrix(Game.width, Game.height, Game.fov, Game.zfar, Game.znear),
		planes = get_frustum_planes(m4transp(camera)),

		pose = Player.entity.pose_matrix(),
		rot = [
			pose[0], pose[1], pose[2],
			pose[4], pose[5], pose[6],
			pose[8], pose[9], pose[10] ],
		
		chunk = Player.chunk(),
		orig = [ Player.entity.x - chunk[0]*CHUNK_X , Player.entity.y - chunk[1]*CHUNK_X, Player.entity.z - chunk[2]*CHUNK_X ],
		
		P, W = new Float32Array(3), 
		basis = Sky.get_basis(),
		n = basis[0], u = basis[1], v = basis[2],
		
		i, j, k,

		//Z-coordinate dimensions		
		z_max = -256.0, z_min = -256.0;		
	
	var pl_x = [ planes[0], planes[1] ],
		pl_y = [ planes[2], planes[3] ],
		pl_z = [ [0, 0, 1, this.z_far],  [0, 0, 1, this.z_near] ];
	
	//Construct the points for the frustum
	for(i=0; i<2; ++i)
	for(j=0; j<2; ++j)
	for(k=0; k<2; ++k)
	{
		var px = pl_x[i],
			py = pl_y[j],
			pz = pl_z[k];
	
		M = m3inv([
			px[0], py[0], pz[0],
			px[1], py[1], pz[1],
			px[2], py[2], pz[2]			
			]);
			
		W = [ -px[3], -py[3], -pz[3] ];
		
		P = m3xform(M, W);
		
		P = m3xform(m3transp(rot), P);

		P[0] += orig[0];
		P[1] += orig[1];
		P[2] += orig[2];
		
		pts.push([dot(P, u), dot(P, v)]);
		
		z_max = Math.max(z_max, dot(P, n));
	}
	

	//Compute center of square and side length
	var dx = pts[3][0] - pts[7][0],
		dy = pts[3][1] - pts[7][1],
		l = Math.sqrt(dx*dx + dy*dy),
		x_min = 10000.0, x_max = -10000.0,
		y_min = 10000.0, y_max = -10000.0,
		px, py;

	dx /= l;
	dy /= l;

	for(k=0; k<pts.length; ++k)
	{
		px =  dx * pts[k][0] + dy * pts[k][1];
		py = -dy * pts[k][0] + dx * pts[k][1];
	
		x_min = Math.min(x_min, px);
		x_max = Math.max(x_max, px);
		y_min = Math.min(y_min, py);
		y_max = Math.max(y_max, py);
	}

	z_max = 256.0;
	z_min = -256.0;
	
	var side = Math.max(x_max - x_min, y_max - y_min),
		ax = dx / side;
		ay = dy / side;
		cx = 0.5 * (x_max + x_min),
		cy = 0.5 * (y_max + y_min),
		z_scale = -1.0 / (z_max - z_min);
	
	return new Float32Array([
		ax*u[0]+ay*v[0],	-ay*u[0]+ax*v[0],	n[0]*z_scale,	0,
		ax*u[1]+ay*v[1],	-ay*u[1]+ax*v[1],	n[1]*z_scale,	0,
		ax*u[2]+ay*v[2],	-ay*u[2]+ax*v[2],	n[2]*z_scale,	0,
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
	
	/*
	gl.enable(gl.CULL_FACE);
	gl.cullFace(gl.FRONT);
	gl.frontFace(gl.CW);
	*/
	
	gl.disable(gl.CULL_FACE);
	
	//Set offset
	gl.polygonOffset(1.1, 4.0);
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
	
	gl.enable(gl.CULL_FACE);
	
	//Generate mipmap
	gl.bindTexture(gl.TEXTURE_2D, this.shadow_tex);
	gl.generateMipmap(gl.TEXTURE_2D);
	gl.bindTexture(gl.TEXTURE_2D, null);
}

ShadowMap.prototype.draw_debug = function(gl)
{
	Debug.draw_tex(this.shadow_tex);
}
