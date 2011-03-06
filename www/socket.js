"use strict";

function encode_session_id(session_id)
{
}

function Socket(portname, session_id, onmessage, onerror)
{
	var url = "ws://127.0.0.1:8081/" + portname + "?=" + encode_session_id(session_id);

	this.onmessage = onmessage;
	this.websocket = new WebSocket(url);
	this.websocket.onmesssage = this.process_recv;
	this.websocket.onclose = onerror;
	this.websocket.onerror = onerror;
}

Socket.prototype.process_recv = function(event)
{
	var arr = new Array(event.data.length);
	for(var i=0; i<data.array_.length; ++i)
	{
		arr[i] = event.data.charCodeAt(i) & 0xff;
	}
	var stream = new PROTO.ByteArrayStream(arr);
	var pbuf = new Network.ServerPacket;
	pbuf.ParseFromStream(stream);
	this.onmessage(pbuf);
}

Socket.prototype.send = function(pbuf)
{
	//Encode protocol buffer
	var data = new PROTO.ByteArrayStream;
	pbuf.SerializeToStream(data);

	//Serialize data to string (bleh)
	var str = "";
	for(var i=0; i<data.array_.length; ++i)
	{
		str += String.fromCharCode(data.array_[i]);
	}

	this.websocket.send(data);
}

