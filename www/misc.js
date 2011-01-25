"use strict";

getShader = function(gl, url)
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

getTexture = function(gl, url)
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

asyncGetBinary = function(url, handler, err_handler, body)
{
	var XHR = new XMLHttpRequest();
	XHR.open("POST", url, true);
	XHR.onreadystatechange = function()
	{
		if(XHR.readyState == 4)
		{
			if(XHR.status == 200 || XHR.status == 304)
			{
				var str = XHR.responseText;
				var arr = new Uint8Array(str.length);
				
				for(var i=0; i<str.length; i++)
				{
					arr[i] = str.charCodeAt(i) & 0xff;
				}
			
				handler(arr);
			}
			else
			{
				handler(new Uint8Array(0));
			}
		}
	}
	XHR.send(body);
}

arr2str = function(arr)
{
	var str = "";
	for(var i=0; i<arr.length; i++)
	{
		str += String.fromCharCode(arr[i] + 0xB0);
	}
	return str;
}

mmult = function(A, B)
{
	var C = new Float32Array(16);
	
	for(var i=0; i<4; i++)
	{
		for(var j=0; j<4; j++)
		{
			x = 0.0
			for(var k=0; k<4; k++)
			{
				x += A[i + 4*k] * B[k + 4*j];
			}
			C[i + 4*j] = x;
		}
	}
	return C;
}

hgmult = function(M, V)
{
	var R = [0, 0, 0, 0];
	
	for(var j=0; j<4; j++)
	{
		for(var i=0; i<4; i++)
		{
			R[i] += M[i+4*j] * V[j]
		}
	}
	
	return R;
}
