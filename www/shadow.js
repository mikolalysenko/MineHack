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
	
	Shadow.shadow_shader.pos_attr = gl.getAttribLocation(Shadow.shadow_shader, "pos");
	if(Shadow.shadow_shader.pos_attr == null)
		return "Could not locate position attribute";

	Shadow.shadow_shader.light_matrix = gl.getUniformLocation(Shadow.shadow_shader, "light");
	if(Shadow.shadow_shader.light_matrix == null)
		return "Could not locate projection matrix uniform";
	
	Shadow.shadow_shader.view = gl.getUniformLocation(Shadow.shadow_shader, "view");
	if(Shadow.shadow_shader.view == null)
		return "Could not locate view matrix uniform";

	
	return "Ok";
}


//A shadow map
var ShadowMap = function(gl, width, height)
{
	this.width		= width;
	this.height		= height;
	this.matrix		= new Float32Array([ 1, 0, 0, 0,
										 0, 1, 0, 0,
										 0, 0, 1, 0,
										 0, 0, 0, 1 ]);

	this.shadow_tex = gl.createTexture();
	gl.bindTexture(gl.TEXTURE_2D, this.shadow_tex);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NONE);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NONE);
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
	gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.vis_tex, 0);
	gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.RENDERBUFFER, this.depth_rb);
	
	if(!gl.isFramebuffer(this.fbo))
	{
		alert("Could not create shadow map frame buffer");
	}

	gl.bindFramebuffer(gl.FRAMEBUFFER, null);
}


ShadowMap.prototype.begin = function(gl, pose)
{
	var light_matrix = mmult(this.matrix, pose);

	gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
	gl.viewport(0, 0, this.width, this.height);
	
	gl.useProgram(Shadows.shadow_shader);
	
	gl.uniformMatrix4fv(Shadows.shadow_shader.light_matrix, false, light_matrix);

	gl.clearColor(0, 0, 0, 0);
	gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
	
	gl.disable(gl.BLEND);
	gl.enable(gl.DEPTH_TEST);
	
	gl.frontFace(gl.CCW);
	gl.enable(gl.CULL_FACE);
	
	gl.enableVertexAttribArray(Shadows.shadow_shader.pos_attr);
}

ShadowMap.prototype.end = function(gl)
{
	gl.bindFramebuffer(gl.FRAMEBUFFER, null);
	
	//Generate mipmap
	gl.bindTexture(gl.TEXTURE_2D, this.shadow_tex);
	gl.generateMipmap(gl.TEXTURE_2D);
	gl.bindTexture(gl.TEXTURE_2D, null);
}

ShadowMap.prototype.draw_debug = function(gl)
{
	Debug.draw_tex(this.shadow_tex);
}
