"use strict";

function printf(str)
{
	if(typeof(console) == "undefined")
	{
		postMessage({type:EV_PRINT, 'str':str});
	}
	else
	{
		console.log(str);
	}
}

function sendProtoBuf_async(pbuf, oncomplete, onerror, timeout_val, async)
{
	if(typeof(timeout_val) == "undefined")
		timeout_val = 5000;

	if(typeof(async) == "undefined")
		async = true;

	//Encode protocol buffer
	var data = new PROTO.ByteArrayStream;
	pbuf.SerializeToStream(data);
	
	//Encode as binary
	var arr = new Uint8Array(data.array_),
		bb = new BlobBuilder,
		XHR = new XMLHttpRequest,
		timeout = setTimeout(function() { XHR.abort(); }, timeout_val);
		
	bb.append(arr.buffer);
	
	//Build request
	XHR.open("POST", "/", async);
	XHR.onreadystatechange = function()
	{
		if(XHR.readyState == 4)
		{
			clearTimeout(timeout);
		
			if(XHR.status == 200 && XHR.responseText.length > 1)
			{
				var str = XHR.responseText,
					arr = new Array(str.length - 1),
					i;
					
				for(i=1; i<str.length; i++)
				{
					arr[i-1] = str.charCodeAt(i) & 0xff;
				}

				var msg = new Network.ServerPacket;
				msg.ParseFromStream(new PROTO.ByteArrayStream(arr));
				oncomplete(msg);
			}
			else
			{
				//Call error handler
				onerror();
			}
		}
	}
	
	//Encode blob
	XHR.send(bb.getBlob("application/octet-stream"));
}


function sendProtoBuf_sync(pbuf, timeout_val)
{
	var result = null;

	sendProtoBuf_async(pbuf, 
		function(msg) { result = msg; },
		function() { },
		timeout_val,
		false);
		
	return result;
}

