"use strict";


function getShader(gl, url)
{
	var script = Loader.data[url];
	if(!script)
		return ["Fail", "Error requesting document"];

	//Extract file extension
	var ext = url.split('.');
	ext = ext[ext.length-1];

	var shader;
	if(ext == 'vs')
	{
		shader = gl.createShader(gl.VERTEX_SHADER);
	}
	else if(ext == 'fs')
	{
		shader = gl.createShader(gl.FRAGMENT_SHADER);
	}
	else
	{
		return ["Fail", "Invalid file extension"];
	}

	gl.shaderSource(shader, script);
	gl.compileShader(shader);

	if(!gl.getShaderParameter(shader, gl.COMPILE_STATUS))
	{
		return ["Fail", gl.getShaderInfoLog(shader)];
	}
	return ["Ok", shader];
}

function getProgram(gl, fs_url, vs_url)
{
	//Load chunk vertex/frag shaders
	var res = getShader(gl, fs_url);
	if(res[0] != "Ok")
		return ["Fail", "Fragment shader " + fs_url + " compile error:\n" + res[1]];
	var fs = res[1];

	res = getShader(gl, vs_url);
	if(res[0] != "Ok")
		return ["Fail", "Vertex shader " + vs_url + " compile error:\n" + res[1]];
	var vs = res[1];

	//Create the chunk shader
	var prog = gl.createProgram();
	gl.attachShader(prog, vs);
	gl.attachShader(prog, fs);
	gl.linkProgram(prog);

	if(!gl.getProgramParameter(prog, gl.LINK_STATUS))
	{
		return ["Fail", "Shader link error (" + fs_url +", " + vs_url + "): Could not link shaders"];
	}

	return ["Ok", fs, vs, prog];
}

function getProgramFromSource(gl, fs_source, vs_source)
{
	var vshader, fshader, prog;

	vshader = gl.createShader(gl.VERTEX_SHADER);
	gl.shaderSource(vshader, vs_source);
	gl.compileShader(vshader);
	
	fshader = gl.createShader(gl.FRAGMENT_SHADER);
	gl.shaderSource(fshader, fs_source);
	gl.compileShader(fshader);
	
	prog = gl.createProgram();
	gl.attachShader(prog, vshader);
	gl.attachShader(prog, fshader);
	gl.linkProgram(prog);

	return prog;
}

function getTexture(gl, url)
{
	var img = Loader.data[url];
	if(!img)
	{
		return ["Fail", "Could not load image " + url];
	}

	var tex = gl.createTexture();
	gl.bindTexture(gl.TEXTURE_2D, tex);
	gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, img);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);

	return ["Ok", tex];	
}
