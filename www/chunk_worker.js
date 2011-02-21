"use strict";

//The chunk worker thread
importScripts(
	'constants.js', 
	'misc.js', 
	'chunk_common.js');

const MAX_NET_CHUNKS	= 512;

const PACK_X_STRIDE		= 1;
const PACK_Y_STRIDE		= CHUNK_X * 3;
const PACK_Z_STRIDE		= CHUNK_X * CHUNK_Y * 9;

var 
	net_pending_chunks = [],				 //Chunks we are waiting for on the network
	vb_pending_chunks = [],					 //Chunks which are waiting for a vertex buffer update
	high_throughput = true,					 //If set, try to maximize throughput at the expense of greater latency.
	wait_chunks = false,					 //If set, we are waiting for more chunks
	packed_buffer = new Array(27*CHUNK_SIZE), //A packed buffer
	empty_data = new Array(CHUNK_SIZE), 	 //Allocate an empty buffer for unloaded chunks
	session_id = new Uint8Array(8),		 	 //Session ID key
	vb_interval = null,						 //Interval timer for vertex buffer generation
	fetch_interval = null;					 //Interval timer for chunk fetch events
	


function pack_buffer(cx, cy, cz)
{
	print("Packing buffer");

	var i, j, k, 
		dx, dy, dz,
		data_offset, buf_offset,
		chunk, data;

	for(dz=-1; dz<=1; ++dz)
	for(dy=-1; dy<=1; ++dy)
	for(dx=-1; dx<=1; ++dx)
	{
		chunk = Map.lookup_chunk(cx+dx, cy+dy, cz+dz);
		if(chunk)
		{
			data = chunk.data;
		}
		else
		{
			data = empty_data;
		}
		
		//Store result into packed buffer
		data_offset = 0;
		for(k=0; k<CHUNK_Z; ++k)
		for(j=0; j<CHUNK_Y; ++j)
		{
			buf_offset  =
					 (dx+1) * CHUNK_X +
					((dy+1) * CHUNK_Y + j) * PACK_Y_STRIDE +
					((dz+1) * CHUNK_Z + k) * PACK_Z_STRIDE;
					
			for(i=0; i<CHUNK_X; ++i)
				packed_buffer[buf_offset++] = data[data_offset++];
		}
	}
	
	//print("Packed: " + packed_buffer);
}



//Decodes a run-length encoded chunk
function decompress_chunk(arr, data)
{
	if(arr.length == 0)
		return -1;

	var i = 0, j, k = 0, l, c;
	while(k<arr.length)
	{
		l = arr[k];
		if(l == 0xff)
		{
			l = arr[k+1] + (arr[k+2] << 8);
			c = arr[k+3];
			k += 4;
		}
		else
		{
			c = arr[k+1];
			k += 2;
		}
		
		if(i + l > CHUNK_SIZE)
		{
			return -1;
		}
		
		for(j=0; j<=l; ++j)
		{
			data[i++] = c;
		}
		
		if(i == CHUNK_SIZE)
			return k;
	}
}


//Retrieves chunks from the network
function grab_chunks()
{
	if(wait_chunks || net_pending_chunks.length == 0)
		return;
	
	var i, j, k = 0,
		chunks = (net_pending_chunks.length > MAX_NET_CHUNKS) ? 
				net_pending_chunks.slice(0,MAX_NET_CHUNKS) : net_pending_chunks,
		base_chunk = chunks[0], 
		base_chunk_id = [base_chunk.x, base_chunk.y, base_chunk.z],
		bb = new BlobBuilder(),
		arr = new Uint8Array(12),
		delta = new Uint8Array(3);	
		
	print("Grabbing chunks");
		
	wait_chunks = true;
	
	if(net_pending_chunks.length > MAX_NET_CHUNKS)
	{
		net_pending_chunks = net_pending_chunks.slice(MAX_NET_CHUNKS);
	}
	else
	{
		net_pending_chunks = [];
	}
	
	bb.append(session_id.buffer);
	
	for(i=0; i<3; ++i)
	{
		arr[k++] = (base_chunk_id[i])		& 0xff;
		arr[k++] = (base_chunk_id[i] >> 8)	& 0xff;
		arr[k++] = (base_chunk_id[i] >> 16)	& 0xff;
		arr[k++] = (base_chunk_id[i] >> 24)	& 0xff;
		
	}
	bb.append(arr.buffer);
	
	//Encode chunk offsets
	for(i=0; i<chunks.length; ++i)
	{
		
		delta[0] = chunks[i].x - base_chunk.x;
		delta[1] = chunks[i].y - base_chunk.y;
		delta[2] = chunks[i].z - base_chunk.z;
		
		bb.append(delta.buffer);
	}
	
	print("Executing XHR");
	
	asyncGetBinary("g", 
	function(arr)
	{
		var i, j, chunk, block, res;
		
		print("Got response: " + arr.length);
	
		wait_chunks = false;
		arr = arr.subarray(1);
	
		for(i=0; i<chunks.length; i++)
		{
			chunk = chunks[i];
			
			//print("Decompressing chunk: " + chunk.x + "," + chunk.y + "," + chunk.z);
			
			//Decompress chunk
			res = -1;			
			if(arr.length > 0)
				res = decompress_chunk(arr, chunk.data);
				
			//print("Res = " + res);
			
			//EOF, clear out remaining chunks
			if(res < 0)
			{
				print("WARNING: FAILED TO DECODE CHUNK");
				for(j=i; j<chunks.length; ++j)
				{
					forget_chunk(chunks[j]);
				}
				return;
			}
			
			//Handle any pending writes
			chunk.pending = false;
			for(j=0; j<chunk.pending_blocks.length; ++j)
			{
				block = chunk.pending_blocks[j];
				chunk.set_block(block[0], block[1], block[2], block[3]);
			}
			delete chunk.pending_blocks;
			
			//Check if chunk is air
			chunk.is_air = true;
			
			var all_stone = true,
				all_air = true;
			
			
			for(j=0; j<CHUNK_SIZE; ++j)
			{
				if(chunk.data[j] != 0)
				{
					all_air = false;
				}
				if(chunk.data[j] != 1)
				{
					all_stone = false;
				}
				
				if(!all_air && !all_stone)
				{
					break;
				}
			}
			
			chunk.is_air = (all_air || all_stone);
			
			//Resize array
			arr = arr.subarray(res);

			//Set dirty flags on neighboring chunks
			set_dirty(chunk.x, chunk.y, chunk.z);
			set_dirty(chunk.x-1, chunk.y, chunk.z);
			set_dirty(chunk.x+1, chunk.y, chunk.z);
			set_dirty(chunk.x, chunk.y-1, chunk.z);
			set_dirty(chunk.x, chunk.y+1, chunk.z);
			set_dirty(chunk.x, chunk.y, chunk.z-1);
			set_dirty(chunk.x, chunk.y, chunk.z+1);
			
			//Send result to main thread
			send_chunk(chunk);
		}
		
		print("Response done");
	}, 
	function()
	{
		var i;
		
		print("ERROR IN XMLHTTPREQUEST!  COULD NOT GET CHUNKS!");
		for(i=0; i<chunks.length; ++i)
		{
			forget_chunk(chunks[i]);
		}

		wait_chunks = false;
	},
	bb.getBlob("application/octet-stream") );
}

//Fetches a chunk
function fetch_chunk(x, y, z)
{
	var str = x + ":" + y + ":" + z, chunk;
	//print("fetching chunk: " + str + ", waiting = " + wait_chunks);
	if(str in Map.index)
		return;
		
	//Create temporary chunk
	chunk = new Chunk(x, y, z);
	chunk.pending = true;
	chunk.dirty = false;
	chunk.pending_blocks = [];
	
	//Add to index
	Map.index[str] = chunk;
	net_pending_chunks.push(chunk);
	
	if(net_pending_chunks.length >= MAX_PENDING_CHUNKS)
	{
		wait_chunks = false;
	}
}


//Marks a chunk as dirty
function set_dirty(x, y, z, priority)
{
	var chunk = Map.lookup_chunk(x, y, z), i;
	
	if(chunk)
	{
		if(chunk.pending)
		{
			return;
		}
		else if(chunk.is_air)
		{
			set_non_pending(chunk.x, chunk.y, chunk.z);
		}
		else if(priority)
		{
			if(chunk.dirty)
			{
				//Find chunk in list of pending chunks and move to front
			}
			else
			{
				chunk.dirty = true;
				vb_pending_chunks.unshift(chunk);
			}
		}
		else if(!chunk.dirty)
		{
			chunk.dirty = true;
			vb_pending_chunks.push(chunk);
		}
	}
}


//Generates all vertex buffers
function generate_vbs()
{
	print("Generating vbs");

	var i, chunk, vbs, nchunks = [];
		
	for(i=0; i < vb_pending_chunks.length; ++i)
	{
		chunk = vb_pending_chunks[i];
		
		if(!chunk.working)
		{
			pack_buffer(chunk.x, chunk.y, chunk.z);
			update_vb(chunk.x, chunk.y, chunk.z);
			chunk.working = true;
			chunk.dirty = false;
		}
		else
		{
			nchunks.push(chunk);
		}
	}
	vb_pending_chunks = nchunks;
}

//Sets a block
function set_block(x, y, z, b)
{
	print("setting block: " + x + "," + y + "," + z + " <- " + b);

	var cx = (x >> CHUNK_X_S), 
		cy = (y >> CHUNK_Y_S), 
		cz = (z >> CHUNK_Z_S),
		bx = (x & CHUNK_X_MASK), 
		by = (y & CHUNK_Y_MASK), 
		bz = (z & CHUNK_Z_MASK),
		flags, i, j, k, idx,
		c = Map.lookup_chunk(cx, cy, cz);		
	if(!c)
		return -1;
		
		
	if(c.pending)
	{
		c.pending_blocks.push([bx,by,bz,b]);
	}
	else
	{
		idx = bx + (by<<CHUNK_X_S) + (bz<<CHUNK_XY_S);
		if(c.data[idx] == b)
			return;

		c.data[idx] = b;
		c.is_air = false;
		c.set_block(bx, by, bz, b);
		
		set_dirty(cx, cy, cz, true);
		
		flags = [ [ bx==0, true, bx==(CHUNK_X-1)],
				  [ by==0, true, by==(CHUNK_Y-1)],
				  [ bz==0, true, bz==(CHUNK_Z-1)] ];
				  
		for(i=0; i<3; ++i)
		for(j=0; j<3; ++j)
		for(k=0; k<3; ++k)
		{
			if(flags[0][i] && flags[1][j] && flags[2][k])
			{
				set_dirty(cx + i-1, cy + j-1, cz + k-1, true);
			}
		}
	}
}

//Starts the worker
function worker_start(key)
{
	print("starting worker");

	session_id.set(key);
	
	print("key = " 
		+ session_id[0] + "," 
		+ session_id[1] + "," 
		+ session_id[2] + "," 
		+ session_id[3] + "," 
		+ session_id[4] + "," 
		+ session_id[5] + "," 
		+ session_id[6] + "," 
		+ session_id[7]);
	
	vb_interval = setInterval(generate_vbs, VB_GEN_RATE);
	fetch_interval = setInterval(grab_chunks, FETCH_RATE);
	
	for(var i=0; i<CHUNK_SIZE; ++i)
	{
		empty_data[i] = 1;
	}
}


//Handles a block update
self.onmessage = function(ev)
{
	//print("got event: " + ev.data + "," + ev.data.type);

	switch(ev.data.type)
	{
		case EV_START:
			worker_start(ev.data.key);
		break;
		
		case EV_SET_BLOCK:
			set_block(ev.data.x, ev.data.y, ev.data.z, ev.data.b);
		break;
		
		case EV_FETCH_CHUNK:
			fetch_chunk(ev.data.x, ev.data.y, ev.data.z);
		break;
		
		case EV_SET_THROTTLE:
			high_throughput = false;
		break;
		
		case EV_VB_COMPLETE:
			vb_gen_complete(ev.data.x, ev.data.y, ev.data.z);
		break;
	}

};

//Handle a completed vb generation round
function vb_gen_complete(x, y, z)
{
	var chunk = Map.lookup_chunk(x, y, z);
	chunk.working = false;
	print("VB Gen complete");
}
			
//Sends an updated set of vertex buffers to the client
function update_vb(x, y, z)
{
	postMessage({ 
		type: EV_VB_UPDATE, 
		'x': x, 'y': y, 'z': z, 
		'data': packed_buffer} );
}

//Sends a new chunk to the client
function send_chunk(chunk)
{
	//Convert data to an array (bleh)
	var tmp_data = new Array(CHUNK_SIZE), i;
	for(i=0; i<CHUNK_SIZE; ++i)
		tmp_data[i] = chunk.data[i];

	postMessage({
		type: EV_CHUNK_UPDATE,
		x:chunk.x, y:chunk.y, z:chunk.z,
		data: tmp_data });
}

function set_non_pending(x, y, z)
{
	postMessage({
		type:EV_SET_NON_PENDING,
		'x':x, 'y':y, 'z':z});
}

//Removes a chunk from the database
function forget_chunk(chunk)
{
	var str = chunk.x + ":" + chunk.y + ":" + chunk.z;
	if(str in Map.index)
	{
		print("Forgetting chunk: " + str);
	
		delete Map.index[str];
	
		postMessage({
			type: EV_FORGET_CHUNK,
			idx: str});
			return;
	}	
}

