"use strict";

//The chunk worker thread
importScripts(
	'constants.js', 
	'misc.js', 
	'chunk_common.js');

const MAX_NET_CHUNKS	= 512;
const VERT_SIZE			= 12;

var 
	vbuffer		= new Array(6*4*CHUNK_SIZE*VERT_SIZE),
	indbuffer	= new Array(6*4*CHUNK_SIZE),
	tindbuffer	= new Array(6*4*CHUNK_SIZE),

	high_throughput = false,					 //Maximizes vertex buffer generation throughput
	net_pending_chunks = [],				 //Chunks we are waiting for on the network
	vb_pending_chunks = [],					 //Chunks which are waiting for a vertex buffer update
	wait_chunks = false,					 //If set, we are waiting for more chunks
	empty_data = new Uint8Array(CHUNK_SIZE), //Allocate an empty buffer for unloaded chunks
	session_id = new Uint8Array(8),		 	 //Session ID key
	vb_interval = null,						 //Interval timer for vertex buffer generation
	fetch_interval = null;					 //Interval timer for chunk fetch events
	
	


//Construct vertex buffer for this chunk
// This code makes me want to barf - Mik
function gen_vb(p)
{
	var 
		v_ptr = 0,
		i_ptr = 0,
		t_ptr = 0,
		
		nv = 0, x, y, z,
	
	//var neighborhood = new Uint32Array(27); (too slow goddammit.  variant arrays even worse.  am forced to do this. hate self.)
		n000, n001, n002,
		n010, n011, n012,
		n020, n021, n022,
		n100, n101, n102,
		n110, n111, n112,
		n120, n121, n122,
		n200, n201, n202,
		n210, n211, n212,
		n220, n221, n222,
	
	//Buffers for scanning	
		buf00, buf01, buf02,
		buf10, buf11, buf12,
		buf20, buf21, buf22,
		
	//Buffers
		data 			= p.data,
		left_buffer		= Map.lookup_chunk(p.x-1, p.y, p.z),
		right_buffer	= Map.lookup_chunk(p.x+1, p.y, p.z),
		
	//Calculates ambient occlusion value
	ao_value = function(s1, s2, c)
	{
		s1 = !Transparent[s1];
		s2 = !Transparent[s2];
		c  = !Transparent[c];
		
		if(s1 && s2)
		{
			return 0.25;
		}
		
		return 1.0 - 0.25 * (s1 + s2 + c);
	},
	
	calc_light = function(
		ox, oy, oz,
		nx, ny, nz,
		s1, s2, c)
	{
		var ao;
		
		ao = ao_value(s1, s2, c);
		
		return [ao, ao, ao];
	},
	
	appendv = function(
		ux, uy, uz,
		vx, vy, vz,
		nx, ny, nz,
		block_id, 
		dir,
		ao00, ao01, ao02,
		ao10, /*ao11,*/ ao12,
		ao20, ao21, ao22)
	{
		var tc, tx, ty, dt,
			ox, oy, oz,
			light,
			orient = nx * (uy * vz - uz * vy) +
					 ny * (uz * vx - ux * vz) +
					 nz * (ux * vy - uy * vx);
	
		if(Transparent[block_id])
		{
			if(orient < 0)
			{
				tindbuffer[t_ptr++] = nv;
				tindbuffer[t_ptr++] = nv+1;
				tindbuffer[t_ptr++] = nv+2;
				
				tindbuffer[t_ptr++] = nv;
				tindbuffer[t_ptr++] = nv+2;
				tindbuffer[t_ptr++] = nv+3;

			}
			else
			{
				tindbuffer[t_ptr++] = nv;
				tindbuffer[t_ptr++] = nv+2;
				tindbuffer[t_ptr++] = nv+1;
				
				tindbuffer[t_ptr++] = nv;
				tindbuffer[t_ptr++] = nv+3;
				tindbuffer[t_ptr++] = nv+2;
			}
		}
		else
		{
			if(orient < 0)
			{
				indbuffer[i_ptr++] = nv;
				indbuffer[i_ptr++] = nv+1;
				indbuffer[i_ptr++] = nv+2;

				indbuffer[i_ptr++] = nv;
				indbuffer[i_ptr++] = nv+2;
				indbuffer[i_ptr++] = nv+3;
			}
			else
			{
				indbuffer[i_ptr++] = nv;
				indbuffer[i_ptr++] = nv+2;
				indbuffer[i_ptr++] = nv+1;

				indbuffer[i_ptr++] = nv;
				indbuffer[i_ptr++] = nv+3;
				indbuffer[i_ptr++] = nv+2;
			}
		}
	
		tc = BlockTexCoords[block_id][dir];
		tx = tc[1] / 16.0;
		ty = tc[0] / 16.0;
		dt = 1.0 / 16.0 - 1.0/256.0;
		
		ox = x - 0.5 + (nx > 0 ? 1 : 0);
		oy = y - 0.5 + (ny > 0 ? 1 : 0);
		oz = z - 0.5 + (nz > 0 ? 1 : 0);

		
		light = calc_light(
			ox, oy, oz,
			nx, ny, nz, 
			ao01, ao10, ao00);
		vbuffer[v_ptr++] = (ox);
		vbuffer[v_ptr++] = (oy);
		vbuffer[v_ptr++] = (oz);
		vbuffer[v_ptr++] = (tx);
		vbuffer[v_ptr++] = (ty+dt);
		vbuffer[v_ptr++] = (nx);
		vbuffer[v_ptr++] = (ny);
		vbuffer[v_ptr++] = (nz);
		vbuffer[v_ptr++] = (light[0]);
		vbuffer[v_ptr++] = (light[1]);
		vbuffer[v_ptr++] = (light[2]);
		vbuffer[v_ptr++] = (0);
		
	
		light = calc_light(
			ox+ux, oy+uy, oz+uz,
			nx, ny, nz, 
			ao01, ao12, ao02);
		vbuffer[v_ptr++] = (ox + ux);
		vbuffer[v_ptr++] = (oy + uy);
		vbuffer[v_ptr++] = (oz + uz);
		vbuffer[v_ptr++] = (tx);
		vbuffer[v_ptr++] = (ty);
		vbuffer[v_ptr++] = (nx);
		vbuffer[v_ptr++] = (ny);
		vbuffer[v_ptr++] = (nz);
		vbuffer[v_ptr++] = (light[0]);
		vbuffer[v_ptr++] = (light[1]);
		vbuffer[v_ptr++] = (light[2]);
		vbuffer[v_ptr++] = (0);

		light = calc_light(
			ox+ux+vx, oy+uy+vz, oz+uz+vz,
			nx, ny, nz, 
			ao12, ao21, ao22);
		vbuffer[v_ptr++] = (ox + ux + vx);
		vbuffer[v_ptr++] = (oy + uy + vy);
		vbuffer[v_ptr++] = (oz + uz + vz);
		vbuffer[v_ptr++] = (tx+dt);
		vbuffer[v_ptr++] = (ty);
		vbuffer[v_ptr++] = (nx);
		vbuffer[v_ptr++] = (ny);
		vbuffer[v_ptr++] = (nz);
		vbuffer[v_ptr++] = (light[0]);
		vbuffer[v_ptr++] = (light[1]);
		vbuffer[v_ptr++] = (light[2]);
		vbuffer[v_ptr++] = (0);

		light = calc_light(
			ox+vx, oy+vy, oz+vz,
			nx, ny, nz, 
			ao10, ao21, ao20);
		vbuffer[v_ptr++] = (ox + vx);
		vbuffer[v_ptr++] = (oy + vy);
		vbuffer[v_ptr++] = (oz + vz);
		vbuffer[v_ptr++] = (tx+dt);
		vbuffer[v_ptr++] = (ty+dt);
		vbuffer[v_ptr++] = (nx);
		vbuffer[v_ptr++] = (ny);
		vbuffer[v_ptr++] = (nz);
		vbuffer[v_ptr++] = (light[0]);
		vbuffer[v_ptr++] = (light[1]);
		vbuffer[v_ptr++] = (light[2]);
		vbuffer[v_ptr++] = (0);
			
		nv += 4;
	},

	get_left_block = function(dy, dz)
	{
		if( dy >= 0 && dy < CHUNK_Y && dz >= 0 && dz < CHUNK_Z )
		{
			if(left_buffer)	
				return left_buffer[CHUNK_X - 1 + (dy<<CHUNK_X_S) + (dz<<(CHUNK_XY_S))];
		}
		else	
		{
			return Map.get_block(
					(p.x<<CHUNK_X_S) - 1,
					dy + (p.y<<CHUNK_Y_S),
					dz + (p.z<<CHUNK_Z_S));
		}
		return 0;
	},
	
	get_right_block = function(dy, dz)
	{
		if( dy >= 0 && dy < CHUNK_Y && dz >= 0 && dz < CHUNK_Z )
		{
			if(right_buffer)	
				return right_buffer[(dy<<CHUNK_X_S) + (dz<<(CHUNK_XY_S))];
		}
		else	
		{
			return Map.get_block(
					((p.x + 1)      <<CHUNK_X_S),
					dy + (p.y<<CHUNK_Y_S),
					dz + (p.z<<CHUNK_Z_S));
		}
		return 0;
	},
	
	get_buf = function(dy, dz)
	{
		if( dy >= 0 && dy < CHUNK_Y &&
			dz >= 0 && dz < CHUNK_Z )
		{
			return data.subarray(
					((dy&CHUNK_Y_MASK)<<CHUNK_X_S) +
					((dz&CHUNK_Z_MASK)<<CHUNK_XY_S) );
		}
		else
		{
			var chunk = Map.lookup_chunk(p.x, p.y + (dy>>CHUNK_Y_S), p.z + (dz>>CHUNK_Z_S));
			if(chunk && !chunk.pending)
			{
				return chunk.data.subarray(
					((dy&CHUNK_Y_MASK)<<CHUNK_X_S) +
					((dz&CHUNK_Z_MASK)<<CHUNK_XY_S) );
			}
		}
		
		return null;
	};
	
	
	if(left_buffer && !left_buffer.pending)
	{
		left_buffer = left_buffer.data;
	}
	else
	{
		left_buffer = null;
	}
	
	if(right_buffer && !right_buffer.pending)
	{
		right_buffer = right_buffer.data;		
	}
	else
	{
		right_buffer = null;
	}
		
	for(z=0; z<CHUNK_Z; ++z)
	for(y=0; y<CHUNK_Y; ++y)
	{
		//Read in center part of neighborhood
		n100 = get_left_block(y-1, z-1);
		n101 = get_left_block(y-1, z);
		n102 = get_left_block(y-1, z+1);
		n110 = get_left_block(y,   z-1);
		n111 = get_left_block(y,   z);
		n112 = get_left_block(y,   z+1);
		n120 = get_left_block(y+1, z-1);
		n121 = get_left_block(y+1, z);
		n122 = get_left_block(y+1, z+1);
		
		//Set up neighborhood buffers
		buf00 = get_buf(y-1, z-1);
		buf01 = get_buf(y-1, z);
		buf02 = get_buf(y-1, z+1);
		buf10 = get_buf(y,   z-1);
		buf11 = get_buf(y,   z);
		buf12 = get_buf(y,   z+1);
		buf20 = get_buf(y+1, z-1);
		buf21 = get_buf(y+1, z);
		buf22 = get_buf(y+1, z+1);
		
		//Read in the right hand neighborhood
		n200 = buf00 ? buf00[0] : 0;
		n201 = buf01 ? buf01[0] : 0;
		n202 = buf02 ? buf02[0] : 0;
		n210 = buf10 ? buf10[0] : 0;
		n211 = buf11 ? buf11[0] : 0;
		n212 = buf12 ? buf12[0] : 0;
		n220 = buf20 ? buf20[0] : 0;
		n221 = buf21 ? buf21[0] : 0;
		n222 = buf22 ? buf22[0] : 0;

	
		for(x=0; x<CHUNK_X; ++x)
		{
			//Shift old 1-neighborhood back by 1 x value
			n000 = n100;
			n001 = n101;
			n002 = n102;
			n010 = n110;
			n011 = n111;
			n012 = n112;
			n020 = n120;
			n021 = n121;
			n022 = n122;
			n100 = n200;
			n101 = n201;
			n102 = n202;
			n110 = n210;
			n111 = n211;
			n112 = n212;
			n120 = n220;
			n121 = n221;
			n122 = n222;
			
			//Fast case: In the middle of an x-scan
			if(x < CHUNK_X - 1)
			{
				if(buf00) n200 = buf00[x+1];
				if(buf01) n201 = buf01[x+1];
				if(buf02) n202 = buf02[x+1];
				if(buf10) n210 = buf10[x+1];
				if(buf11) n211 = buf11[x+1];
				if(buf12) n212 = buf12[x+1];
				if(buf20) n220 = buf20[x+1];
				if(buf21) n221 = buf21[x+1];
				if(buf22) n222 = buf22[x+1];
			}
			else	//Bad case, end of x-scan
			{
				//Read in center part of neighborhood
				n200 = get_right_block(y-1, z-1);
				n201 = get_right_block(y-1, z);
				n202 = get_right_block(y-1, z+1);
				n210 = get_right_block(y,   z-1);
				n211 = get_right_block(y,   z);
				n212 = get_right_block(y,   z+1);
				n220 = get_right_block(y+1, z-1);
				n221 = get_right_block(y+1, z);
				n222 = get_right_block(y+1, z+1);
			}

			if(n111 == 0)
				continue;
				
				
			if(Transparent[n011] && n011 != n111)
			{
				appendv( 
					 0,  1,  0,
					 0,  0,  1,
					-1,  0,  0,
					n111, 1,
					n000,  n010,  n020,
					n001,/*n011,*/n021,
					n002,  n012,  n022);
			}
			
			if(Transparent[n211] && n211 != n111)
			{
				appendv( 
					 0,  1,  0,
					 0,  0,  1,
					 1,  0,  0,
					n111, 1,
					n200,  n210,  n220,
					n201,/*n211,*/n221,
					n202,  n212,  n222);
			}
			
			if(Transparent[n101] && n101 != n111)
			{
				appendv( 
					 0,  0,  1,
					 1,  0,  0,
					 0, -1,  0,
					n111, 2,
					n000,  n001,  n002,
					n100,/*n101,*/n102,
					n200,  n201,  n202);
			}
			
			
			if(Transparent[n121] && n121 != n111)
			{
				appendv( 
					 0,  0,  1,
 					 1,  0,  0,
					 0,  1,  0,
					n111, 0,
					n020,  n021,  n022,
					n120,/*n121,*/n122,
					n220,  n221,  n222);
			}
			

			if(Transparent[n110] && n110 != n111)
			{
				appendv( 
					 0, 1,  0,
					 1,  0,  0,
					 0,  0, -1,
					n111, 1,
					n000,  n010,  n020,
					n100,/*n110,*/n120,
					n200,  n210,  n220);
			}

			if(Transparent[n112] && n112 != n111)
			{
				appendv( 
					 0, 1,  0,
					 1,  0,  0,
					 0,  0, 1,
					n111, 1,
					n002,  n012,  n022,
					n102,/*n112,*/n122,
					n202,  n212,  n222);
			}
		}
	}
	
	//Return result	
	return [vbuffer.slice(0, v_ptr), indbuffer.slice(0, i_ptr), tindbuffer.slice(0, t_ptr)];
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
			for(j=0; j<CHUNK_SIZE; ++j)
			{
				if(chunk.data[j] != 0)
				{
					chunk.is_air = false;
					break;
				}
			}
			
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
			send_vb(chunk.x, chunk.y, chunk.z, [[], [], []]);
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

	var i, chunk, vbs;
		
	for(i=0; i < Math.min(vb_pending_chunks.length, MAX_VB_UPDATES); ++i)
	{
		chunk = vb_pending_chunks[i];
		//print("generating vb: " + chunk.x + "," + chunk.y + "," + chunk.z);
		send_vb(chunk.x, chunk.y, chunk.z, gen_vb(chunk));
		chunk.dirty = false;
	}
	vb_pending_chunks = vb_pending_chunks.slice(i);
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
		empty_data[i] = 0;
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
	}

};

//Sends an updated set of vertex buffers to the client
function send_vb(x, y, z, vbs)
{
	postMessage({ 
		type: EV_VB_UPDATE, 
		'x': x, 'y': y, 'z': z, 
		verts: vbs[0],
		ind: vbs[1],
		tind: vbs[2]});
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

