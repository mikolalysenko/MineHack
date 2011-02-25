"use strict";

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


function asyncGetBinary(url, handler, err_handler, body)
{
	var XHR = new XMLHttpRequest();
	
	XHR.open("POST", url, true);
	XHR.onreadystatechange = function()
	{
		print("ready state = " + XHR.readyState);
	
		if(XHR.readyState == 4)
		{
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

function arr2str(arr)
{
	var str = "";
	for(var i=0; i<arr.length; i++)
	{
		str += String.fromCharCode(arr[i] + 0xB0);
	}
	return str;
}
