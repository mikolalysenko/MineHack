"use strict";

//The chunk worker thread
importScripts(
	'constants.js', 
	'misc.js', 
	'chunk_common.js');

var MAX_NET_CHUNKS		= 512,
	VERT_SIZE			= 12,

	PACK_X_STRIDE		= 1,
	PACK_Y_STRIDE		= CHUNK_X * 3,
	PACK_Z_STRIDE		= CHUNK_X * CHUNK_Y * 9,

	PACK_X_OFFSET		= CHUNK_X,
	PACK_Y_OFFSET		= CHUNK_Y,
	PACK_Z_OFFSET		= CHUNK_Z;

var 
	vb_pending_chunks = [],					 //Chunks which are waiting for a vertex buffer update
	wait_chunks = false,					 //If set, we are waiting for more chunks
	packed_buffer = new Array(27*CHUNK_SIZE), //A packed buffer
	empty_data = new Array(CHUNK_SIZE), 	 //Allocate an empty buffer for unloaded chunks
	session_id = new Uint8Array(8),		 	 //Session ID key
	vb_interval = null,						 //Interval timer for vertex buffer generation
	fetch_interval = null,					 //Interval timer for chunk fetch events
	
	v_ptr = 0,
	i_ptr = 0,
	t_ptr = 0,
	nv = 0,
	
	vbuffer		= new Array(6*4*CHUNK_SIZE*VERT_SIZE),
	indbuffer	= new Array(6*4*CHUNK_SIZE),
	tindbuffer	= new Array(6*4*CHUNK_SIZE);
	
//Retrieves a pointer into a packed buffer
function get_ptr(j, k)
{
	return PACK_X_OFFSET-1 +
		(PACK_Y_OFFSET+j)*PACK_Y_STRIDE +
		(PACK_Z_OFFSET+k)*PACK_Z_STRIDE;
}

//Calculates ambient occlusion value
function ao_value(s1, s2, c)
{
	s1 = !Transparent[s1];
	s2 = !Transparent[s2];
	c  = !Transparent[c];
	
	if(s1 && s2)
	{
		return 0.25;
	}
	
	return 1.0 - 0.25 * (s1 + s2 + c);
}

//Calculates the light value
function calc_light(
	ox, oy, oz,
	nx, ny, nz,
	s1, s2, c)
{
	var ao;
	
	ao = ao_value(s1, s2, c);
	
	return [ao, ao, ao];
}

//Construct vertex buffer for this chunk
// This code makes me want to barf - Mik
function gen_vb()
{
	var 
		x=0, y=0, z=0,
	
	//var neighborhood = new Uint32Array(27); (too slow goddammit.  variant arrays even worse.  am forced to do this. hate self.)
		n000=0, n001=0, n002=0,
		n010=0, n011=0, n012=0,
		n020=0, n021=0, n022=0,
		n100=0, n101=0, n102=0,
		n110=0, n111=0, n112=0,
		n120=0, n121=0, n122=0,
		n200=0, n201=0, n202=0,
		n210=0, n211=0, n212=0,
		n220=0, n221=0, n222=0,
	
	//Buffers for scanning	
		p00=0, p01=0, p02=0,
		p10=0, p11=0, p12=0,
		p20=0, p21=0, p22=0;
	
	//Initialize vertex buffer pointers
	v_ptr = 0;
	i_ptr = 0;
	t_ptr = 0;
	nv = 0;
	
	
	function appendv(
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
	}
		
	for(z=0; z<CHUNK_Z; ++z)
	for(y=0; y<CHUNK_Y; ++y)
	{
		//Set up neighborhood buffers
		p00 = get_ptr(y-1, z-1);
		p01 = get_ptr(y-1, z);
		p02 = get_ptr(y-1, z+1);
		p10 = get_ptr(y,   z-1);
		p11 = get_ptr(y,   z);
		p12 = get_ptr(y,   z+1);
		p20 = get_ptr(y+1, z-1);
		p21 = get_ptr(y+1, z);
		p22 = get_ptr(y+1, z+1);


		//Read in initial neighborhood
		n100 = packed_buffer[p00++];
		n101 = packed_buffer[p01++];
		n102 = packed_buffer[p02++];
		n110 = packed_buffer[p10++];
		n111 = packed_buffer[p11++];
		n112 = packed_buffer[p12++];
		n120 = packed_buffer[p20++];
		n121 = packed_buffer[p21++];
		n122 = packed_buffer[p22++];
		
		n200 = packed_buffer[p00++];
		n201 = packed_buffer[p01++];
		n202 = packed_buffer[p02++];
		n210 = packed_buffer[p10++];
		n211 = packed_buffer[p11++];
		n212 = packed_buffer[p12++];
		n220 = packed_buffer[p20++];
		n221 = packed_buffer[p21++];
		n222 = packed_buffer[p22++];

		
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
			
			//Read in next neighborhood
			n200 = packed_buffer[p00++];
			n201 = packed_buffer[p01++];
			n202 = packed_buffer[p02++];
			n210 = packed_buffer[p10++];
			n211 = packed_buffer[p11++];
			n212 = packed_buffer[p12++];
			n220 = packed_buffer[p20++];
			n221 = packed_buffer[p21++];
			n222 = packed_buffer[p22++];

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



function pack_buffer(cx, cy, cz)
{
	//print("Packing buffer");

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


function unpack_vis_buffer(arr)
{
	wait_chunks = false;
	if(arr.length == 0)
		return;
		
	var cx = (arr[0] + (arr[1]<<8) + ((arr[2] & 15)<<16))*8,
		cy = ((arr[2]>>4) + (arr[3]<<4) + ((arr[4]&63)<<12))*4,
		cz = (arr[5] + (arr[6]<<8) + (arr[7]<<16))*8,
		count = arr[8],
		chunk_ids = arr.subarray(9),
		chunk_data = arr.subarray(9+count);
		
	
	print("Base chunk = " + cx + ',' + cy + ',' + cz);
	
	for(var i=0; i<count; ++i)
	{
		var offs = chunk_ids[i],
			ox = cx + (offs&7),
			oy = cy + ((offs>>3)&3),
			oz = cz + (offs>>5),
				
			chunk = Map.add_chunk(ox, oy, oz),
			res = -1;
			
			
		//print("Got chunk: " + ox + "," + oy + "," + oz);
			
		if(chunk_data.length > 0)
			res = decompress_chunk(chunk_data, chunk.data);
			
		if(res < 0)
		{
			print("Failure!  Could not read chunk");
			return;
		}
			
		//Handle any pending writes
		chunk.pending = false;
		chunk.is_air = false;

		//Resize array
		chunk_data = chunk_data.subarray(res);

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
}

function grab_vis_buffer()
{
	if(wait_chunks)
		return;

	print("Grabbing visible chunks");
	
	
	var bb = new BlobBuilder();
	bb.append(session_id.buffer);
	wait_chunks = true;
	
	asyncGetBinary("v", unpack_vis_buffer,
	function()
	{
		wait_chunks = false;
		print("Error retreiving vis buffer");
	},
	bb.getBlob("application/octet-stream"));
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
//	print("Generating vbs");

	var i, chunk, vbs = [];
	for(i=0; i < Math.min(vb_pending_chunks.length, MAX_VB_UPDATES); ++i)
	{
		chunk = vb_pending_chunks[i];
		pack_buffer(chunk.x, chunk.y, chunk.z);
		vbs.push({'x':chunk.x, 'y':chunk.y, 'z':chunk.z, 'd':gen_vb()});
		chunk.dirty = false;
	}	
	send_vb(vbs);
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
	fetch_interval = setInterval(grab_vis_buffer, 800);

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
	}

};

//Sends an updated set of vertex buffers to the client
function send_vb(vbs)
{
	postMessage({ type: EV_VB_UPDATE, 'vbs':vbs });
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
