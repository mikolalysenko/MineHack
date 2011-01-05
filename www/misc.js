getDocument = function(url)
{
	XHR = new XMLHttpRequest();
	XHR.open("GET", url, false);
	XHR.send(null);
	return XHR.responseText;
}

getShader = function(gl, url)
{
	var script = getDocument(url);
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

getProgram = function(gl, fs_url, vs_url)
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
