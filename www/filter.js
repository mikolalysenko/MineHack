//A 2-pass separable box filter
function Filter(width, height, r)
{


	res = getProgram(gl, "shaders/shadow_filter.fs", "shaders/shadow_filter.vs");
	
	if(res[0] != "Ok")
		return res[1];
	
	Shadows.blur_shader	= res[3];
	Shadows.blur_shader.pos_attr = gl.getAttribLocation(Shadows.blur_shader, "pos");
	if(Shadows.blur_shader.pos_attr == null)
		return "Could not locate position attribute for shadow init shader";
	
	Shadows.blur_tex = gl.createTexture();
	gl.bindTexture(gl.TEXTURE_2D, Shadows.blur_tex);
	gl.texParameteri(gl.TEXTURE_2D,	gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
	gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 256, 256, 0, gl.RGBA, gl.FLOAT, null);
	gl.bindTexture(gl.TEXTURE_2D, null);	

}


Filter.prototype.execute = function(fbo)
{
}
