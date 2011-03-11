"use strict";

function pad_hex(w)
{
	var r = Number(w).toString(16);
	while(r.length < 16)
	{
		r = "0" + r;
	}
	return r;
}

function encode_session_id(lsw, msw)
{
	return pad_hex(msw) + pad_hex(lsw);
}

function Socket(portname, session_id_lsw, session_id_msw, onmessage, onerror)
{
	var url = "ws://127.0.0.1:8081/" + portname + "?=" + encode_session_id(session_id_lsw, session_id_msw);

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
	this.websocket.send(pbuf_to_raw(pbuf));
}

Socket.prototype.send_from_raw = function(raw)
{
	this.websocket.send(raw);
}

