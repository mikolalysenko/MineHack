"use strict";

importScripts(
	'constants.js', 
	'protobuf.js',
	'pbj.js',
	'misc.js');

//Member variables
var socket;

//Sends the raw data to the socket
function on_send(arr)
{
	var str = "";
	for(var i=0; i<data.array_.length; ++i)
	{
		str += String.fromCharCode(arr[i]);
	}

	socket.send(arr);
}

//Called when receiving a protocol buffer
function on_recv(event)
{
	var arr = new Array(event.data.length);
	for(var i=0; i<data.array_.length; ++i)
	{
		arr[i] = event.data.charCodeAt(i) & 0xff;
	}

	postMessage({type:EV_RECV_PBUF, 'raw':arr});
}

//Socket error
function on_socket_error()
{
	postMessage({type:EV_CRASH});
}

//Starts the worker
function worker_start(lsw, msw)
{
	function pad_hex(w)
	{
		var r = Number(w).toString(16);
		while(r.length < 8)
		{
			r = "0" + r;
		}
		return r;
	}

	socket = new WebSocket("ws://"+DOMAIN_NAME+"/update/"+pad_hex(msw)+pad_hex(lsw));
	socket.onmessage = on_recv;
	socket.onerror = on_socket_error;
	socket.onclose = on_socket_error;
}

//Handles a block update
self.onmessage = function(ev)
{
	//print("got event: " + ev.data + "," + ev.data.type);

	switch(ev.data.type)
	{
		case EV_START:
			worker_start(ev.data.lsw, ev.data.msw);
		break;
		
		case EV_SEND:
			on_send(ev.data.raw);
		break;
	}
};
