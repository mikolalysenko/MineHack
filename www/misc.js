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
