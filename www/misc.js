function print(str)
{
	if(typeof(console) == 'undefined')
	{
		postMessage({type:EV_PRINT, 'str':str});
	}
	else
	{
		//console.log(str);
	}
}


asyncGetBinary = function(url, handler, err_handler, body)
{
	var XHR = new XMLHttpRequest(),
		timer = setTimeout(function()
		{
			if(XHR.readyState != 4)
			{
				XHR.abort();
				err_handler();
			}
		}, 5000);
	
	XHR.open("POST", url, true);
	XHR.onreadystatechange = function()
	{
		if(XHR.readyState == 4)
		{
			clearTimeout(timer);
		
			if(XHR.status == 200)
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
				err_handler();
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
	var R = new Float32Array([0, 0, 0, 0]), i, j;
	
	for(j=0; j<4; ++j)
	{
		for(i=0; i<4; ++i)
		{
			R[i] += M[i+4*j] * V[j]
		}
	}
	
	/*
	for(i=0; i<4; ++i)
	{
		R[i] /= R[3];
	}
	*/
	
	return R;
}


function minv3(mat)
{
	var k,
	
	M = function(i, j) { return mat[i + 3*j]; },
	
	R = new Float32Array([
		 M(1,1)*M(2,2)-M(1,2)*M(2,1),	-M(1,0)*M(2,2)+M(1,2)*M(2,0),	 M(1,0)*M(2,1)-M(1,1)*M(2,0),
		-M(0,1)*M(2,2)+M(0,2)*M(2,1),	 M(0,0)*M(2,2)-M(0,2)*M(2,0),	-M(0,0)*M(2,1)+M(0,1)*M(2,0),
		 M(0,1)*M(1,2)-M(0,2)*M(1,1),	-M(0,0)*M(1,2)+M(0,2)*M(1,0),	 M(0,0)*M(1,1)-M(0,1)*M(1,0) ]),
	
	D = M(0,0) * R[0]  + M(0,1) * R[1] + M(0,2) * R[2];
	
	if(Math.abs(D) < 0.0001)
	{
		return new Float32Array(9);
	}
	
	for(k=0; k<9; ++k)
	{
		R[k] /= D;
	}
	
	return R;
}

function m3xform(M, x)
{
	var i, j, y = new Float32Array(3);
	
	for(i=0; i<3; ++i)
	for(j=0; j<3; ++j)
	{
		y[i] += M[i+3*j] * x[j];
	}
	
	return y;
}

function m3transp(M)
{
	var res = new Float32Array(9);
	
	for(i=0; i<3; ++i)
	for(j=0; j<3; ++j)
	{
		res[i+3*j] = M[j+3*i];
	}
	
	return res;
}

function m4det(m)
{
	return  m[3]*m[6]*m[9]*m[12] - m[2]*m[7]*m[9]*m[12] - m[3]*m[5]*m[10]*m[12] + m[1]*m[7]*m[10]*m[12]+
			m[2]*m[5]*m[11]*m[12] - m[1]*m[6]*m[11]*m[12] - m[3]*m[6]*m[8]*m[13] + m[2]*m[7]*m[8]*m[13]+
			m[3]*m[4]*m[10]*m[13] - m[0]*m[7]*m[10]*m[13] - m[2]*m[4]*m[11]*m[13] + m[0]*m[6]*m[11]*m[13]+
			m[3]*m[5]*m[8]*m[14] - m[1]*m[7]*m[8]*m[14] - m[3]*m[4]*m[9]*m[14] + m[0]*m[7]*m[9]*m[14]+
			m[1]*m[4]*m[11]*m[14] - m[0]*m[5]*m[11]*m[14] - m[2]*m[5]*m[8]*m[15] + m[1]*m[6]*m[8]*m[15]+
			m[2]*m[4]*m[9]*m[15] - m[0]*m[6]*m[9]*m[15] - m[1]*m[4]*m[10]*m[15] + m[0]*m[5]*m[10]*m[15];
}

function m4inv(m)
{
	var res = new Float32Array([
		m[6]*m[11]*m[13] - m[7]*m[10]*m[13] + m[7]*m[9]*m[14] - m[5]*m[11]*m[14] - m[6]*m[9]*m[15] + m[5]*m[10]*m[15],
		m[3]*m[10]*m[13] - m[2]*m[11]*m[13] - m[3]*m[9]*m[14] + m[1]*m[11]*m[14] + m[2]*m[9]*m[15] - m[1]*m[10]*m[15],
		m[2]*m[7]*m[13] - m[3]*m[6]*m[13] + m[3]*m[5]*m[14] - m[1]*m[7]*m[14] - m[2]*m[5]*m[15] + m[1]*m[6]*m[15],
		m[3]*m[6]*m[9] - m[2]*m[7]*m[9] - m[3]*m[5]*m[10] + m[1]*m[7]*m[10] + m[2]*m[5]*m[11] - m[1]*m[6]*m[11],
		m[7]*m[10]*m[12] - m[6]*m[11]*m[12] - m[7]*m[8]*m[14] + m[4]*m[11]*m[14] + m[6]*m[8]*m[15] - m[4]*m[10]*m[15],
		m[2]*m[11]*m[12] - m[3]*m[10]*m[12] + m[3]*m[8]*m[14] - m[0]*m[11]*m[14] - m[2]*m[8]*m[15] + m[0]*m[10]*m[15],
		m[3]*m[6]*m[12] - m[2]*m[7]*m[12] - m[3]*m[4]*m[14] + m[0]*m[7]*m[14] + m[2]*m[4]*m[15] - m[0]*m[6]*m[15],
		m[2]*m[7]*m[8] - m[3]*m[6]*m[8] + m[3]*m[4]*m[10] - m[0]*m[7]*m[10] - m[2]*m[4]*m[11] + m[0]*m[6]*m[11],
		m[5]*m[11]*m[12] - m[7]*m[9]*m[12] + m[7]*m[8]*m[13] - m[4]*m[11]*m[13] - m[5]*m[8]*m[15] + m[4]*m[9]*m[15],
		m[3]*m[9]*m[12] - m[1]*m[11]*m[12] - m[3]*m[8]*m[13] + m[0]*m[11]*m[13] + m[1]*m[8]*m[15] - m[0]*m[9]*m[15],
		m[1]*m[7]*m[12] - m[3]*m[5]*m[12] + m[3]*m[4]*m[13] - m[0]*m[7]*m[13] - m[1]*m[4]*m[15] + m[0]*m[5]*m[15],
		m[3]*m[5]*m[8] - m[1]*m[7]*m[8] - m[3]*m[4]*m[9] + m[0]*m[7]*m[9] + m[1]*m[4]*m[11] - m[0]*m[5]*m[11],
		m[6]*m[9]*m[12] - m[5]*m[10]*m[12] - m[6]*m[8]*m[13] + m[4]*m[10]*m[13] + m[5]*m[8]*m[14] - m[4]*m[9]*m[14],
		m[1]*m[10]*m[12] - m[2]*m[9]*m[12] + m[2]*m[8]*m[13] - m[0]*m[10]*m[13] - m[1]*m[8]*m[14] + m[0]*m[9]*m[14],
		m[2]*m[5]*m[12] - m[1]*m[6]*m[12] - m[2]*m[4]*m[13] + m[0]*m[6]*m[13] + m[1]*m[4]*m[14] - m[0]*m[5]*m[14],
		m[1]*m[6]*m[8] - m[2]*m[5]*m[8] + m[2]*m[4]*m[9] - m[0]*m[6]*m[9] - m[1]*m[4]*m[10] + m[0]*m[5]*m[10] ]),
	
	d = m4det(m), i;
	
	for(i=0; i<16; ++i)
		res[i] /= d;
	return res;
}

function m4transp(M)
{
	var res = new Float32Array(16);
	
	for(i=0; i<4; ++i)
	for(j=0; j<4; ++j)
	{
		res[i+4*j] = M[j+4*i];
	}
	
	return res;
}

function cross(u, v)
{
	return [ 
		u[1] * v[2] - u[2] * v[1],
		u[2] * v[0] - u[0] * v[2],
		u[0] * v[1] - u[1] * v[0] ];
}

function dot(u, v)
{
	return u[0] * v[0] + u[1] * v[1] + u[2] * v[2];
}

