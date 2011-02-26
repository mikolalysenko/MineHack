"use strict";

function printf(str)
{
	if(typeof(console) == 'undefined')
	{
		postMessage({type:EV_PRINT, 'str':str});
	}
	else
	{
		console.log(str);
	}
}


function sendProtoBuf(pbuf, oncomplete, onerror, timeout)
{
	//Encode protocol buffer
	var data = new PROTO.ByteArrayStream;
	pbuf.SerializeToStream(data);
	
	//Compress data
	
	var compressed = RawDeflate.deflate(data.array_),
		arr = new Uint8Array(compressed),
		bb = new BlobBuilder;
	bb.append(arr.buffer);
	
	//Build request
	var XHR = new XMLHttpRequest;
	
	XHR.open("POST", "/", true);
	XHR.onreadystatechange = function()
	{
		if(XHR.readyState == 4)
		{
			if(XHR.status == 200)
			{
				var str = XHR.responseText,
					arr = new Array(str.length),
					i;
					
				for(i=0; i<str.length; i++)
				{
					arr[i] = str.charCodeAt(i) & 0xff;
				}

				var msg = new Network.ServerPacket;
				msg.ParseFromStream(new PROTO.ByteArrayStream(arr));
				
				//Call handler
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
