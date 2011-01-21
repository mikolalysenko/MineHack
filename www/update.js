var UpdateHandler = {};

var decode_int = function(arr, i)
{
	return arr[i] + (arr[i+1]<<8) + (arr[i+2]<<16) + (arr[i+3]<<24);
}

var decode_ushort = function(arr, i)
{
	return arr[i] + (arr[i+1]<<8);
}

var decode_short = function(arr, i)
{
	var v = arr[i] + (arr[i+1]<<8);
	if(v >= (1<<15))
		v = (1<<16) - v;
	return v;
}

var decode_byte = function(arr, i)
{
	var b = arr[i];
	if(b >= (1<<7))
		b = (1<<8) - b;
	return b;
}


//Handles an update packet from the server
UpdateHandler.handle_update_packet = function(arr)
{
	Game.wait_for_heartbeat = false;
	
	//Check header length
	if(arr.length < NET_HEADER_SIZE)
		return;
	
	//Parse packet header
	Game.tick_lo = decode_int(arr, 0);
	Game.tick_hi = decode_int(arr, 4);
	
	var ox = decode_int(arr,8),
		oy = decode_int(arr,12),
		oz = decode_int(arr,16),
		block_size	= decode_ushort(arr, 20),
		chat_size	= decode_ushort(arr, 22),
		coord_size	= decode_ushort(arr, 24),
		update_size	= decode_ushort(arr, 26),
		kill_size	= decode_ushort(arr, 28);


	if(arr.length < NET_HEADER_SIZE + 4 * block_size + chat_size +  9 * coord_size + update_size +  8 * kill_size)
		return;

	//Parse out block updates
	var idx = NET_HEADER_SIZE;
	
	while(--block_size>=0)
	{
		var x = decode_byte(arr, idx++) + ox;
		var y = decode_byte(arr, idx++) + oy;
		var z = decode_byte(arr, idx++) + oz;
		var b = arr[idx++];
		
		Map.set_block(x, y, z, b);
	}
	
	//Append chat log
	var chat_uint = arr.slice(idx, idx+chat_size);
	idx += chat_size;
	var chat_utf = "";	//Have to convert to UTF-8
	for(var i=0; i<chat_uint.length; i++)
		chat_utf += String.fromCharCode(chat_uint[i] & 0x7f);
	document.getElementById("chatLog").innerHTML += Utf8.decode(chat_utf);
	
	//Parse out coordinates
	var coords = [];
	
	while(--coord_size>=0)
	{
		var x = decode_short(arr, idx) * NET_DIVIDE + ox;
		idx += 2;
		var y = decode_short(arr, idx) * NET_DIVIDE + oy;
		idx += 2;
		var z = decode_short(arr, idx) * NET_DIVIDE + oz;
		idx += 2;
		
		var pitch	= arr[idx++] * 2.0 * Math.PI / 255.0;
		var yaw 	= arr[idx++] * 2.0 * Math.PI / 255.0;
		var roll	= arr[idx++] * 2.0 * Math.PI / 255.0;
		
		coords.push( [ x, y, z, pitch, yaw, roll ] );
	}
	
	//Parse out entity updates
	var pidx = idx;
	for(var n=0; idx - pidx < update_size; ++n)
	{
		//Parse out entity data
		var initialize = arr[idx++];
		var entity_id = arr2str(arr.slice(idx, idx+8));
		idx += 8;
		
		var r = -1;
		
		if(initialize)
		{
			//Send update packet to the target
			r = EntityDB.create_entity(entity_id, coords[n], arr.slice(idx));
		}
		else
		{
			r = EntityDB.update_entity(entity_id, coords[n], arr.slice(idx));
		}
		
		if(r < 0)
		{
			alert("Bad entity packet");
			break;
		}
		
		idx += r;
	}
	
	if(idx != pidx + update_size)
		alert("PACKET OVERFLOWED!!!!");
	
	//Rebase index pointer in case something went wrong
	idx = pidx + update_size;
	while(--kill_size >= 0)
	{
		EntityDB.destroy_entity(arr2str(arr.slice(idx, idx+8)))
		idx += 8;
	}
}

