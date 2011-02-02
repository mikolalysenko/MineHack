//The chunk worker thread
importScripts(
	'constants.js', 
	'misc.js', 
	'chunk_data.js');

var net_pending_chunks = {},				 //Chunks we are waiting for on the network
	vb_pending_chunks = {},					 //Chunks which are waiting for a vertex buffer update
	wait_chunks = false,					 //If set, we are waiting for more chunks
	empty_data = new Uint8Array(CHUNK_SIZE), //Allocate an empty buffer for unloaded chunks
	session_key;
	
	

//Sets a block in the map to the given type
Map.set_block = function(x, y, z, b)
{
	var cx = (x >> CHUNK_X_S), 
		cy = (y >> CHUNK_Y_S), 
		cz = (z >> CHUNK_Z_S);
	var c = Map.lookup_chunk(cx, cy, cz);		
	if(!c)
		return -1;
		
	//Need to update the height field	
	var bx = (x & CHUNK_X_MASK), 
		by = (y & CHUNK_Y_MASK), 
		bz = (z & CHUNK_Z_MASK);
	return c.set_block(bx, by, bz, b);
}

//Construct vertex buffer for this chunk
// This code makes me want to barf - Mik
function gen_vb(p)
{
	var vertices = [],
		indices  = [],
		tindices = [],
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
		b00, b01, b02,
		b10, b11, b12,
		b20, b21, b22,
	
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
			orient = nx * (uy * vz - uz * vy) +
					 ny * (uz * vx - ux * vz) +
					 nz * (ux * vy - uy * vx);
	
		if(Transparent[block_id])
		{
			if(orient < 0)
			{
				tindices.push(nv);
				tindices.push(nv+1);
				tindices.push(nv+2);
				tindices.push(nv);
				tindices.push(nv+2);
				tindices.push(nv+3);
			}
			else
			{
				tindices.push(nv);
				tindices.push(nv+2);
				tindices.push(nv+1);
				tindices.push(nv);
				tindices.push(nv+3);
				tindices.push(nv+2);				
			}
		}
		else
		{
			if(orient < 0)
			{
				indices.push(nv);
				indices.push(nv+1);
				indices.push(nv+2);
				indices.push(nv);
				indices.push(nv+2);
				indices.push(nv+3);
			}
			else
			{
				indices.push(nv);
				indices.push(nv+2);
				indices.push(nv+1);
				indices.push(nv);
				indices.push(nv+3);
				indices.push(nv+2);			
			}
		}
	
		tc = BlockTexCoords[block_id][dir];
		tx = tc[1] / 16.0;
		ty = tc[0] / 16.0;
		dt = 1.0 / 16.0 - 1.0/256.0;
		
		ox = x - 0.5 + (nx > 0 ? 1 : 0);
		oy = y - 0.5 + (ny > 0 ? 1 : 0);
		oz = z - 0.5 + (nz > 0 ? 1 : 0);

		vertices.push(ox);
		vertices.push(oy);
		vertices.push(oz);
		vertices.push(1);
		vertices.push(tx);
		vertices.push(ty+dt);
		vertices.push(ao_value(ao01, ao10, ao00));
		vertices.push(0);
	
		vertices.push(ox + ux);
		vertices.push(oy + uy);
		vertices.push(oz + uz);
		vertices.push(1);
		vertices.push(tx);
		vertices.push(ty);
		vertices.push(ao_value(ao01, ao12, ao02));
		vertices.push(0);

		vertices.push(ox + ux + vx);
		vertices.push(oy + uy + vy);
		vertices.push(oz + uz + vz);
		vertices.push(1);
		vertices.push(tx+dt);
		vertices.push(ty);
		vertices.push(ao_value(ao12, ao21, ao22));
		vertices.push(0);

		vertices.push(ox + vx);
		vertices.push(oy + vy);
		vertices.push(oz + vz);
		vertices.push(1);
		vertices.push(tx+dt);
		vertices.push(ty+dt);
		vertices.push(ao_value(ao10, ao21, ao20));
		vertices.push(0);
			
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
		return 0xff;
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
		return 0xff;
	},
	
	get_buf = function(dy, dz)
	{
		if( dy >= 0 && dy < CHUNK_Y &&
			dz >= 0 && dz < CHUNK_Z )
		{
			return data.slice(
					((dy&CHUNK_Y_MASK)<<CHUNK_X_S) +
					((dz&CHUNK_Z_MASK)<<CHUNK_XY_S) );
		}
		else
		{
			var chunk = Map.lookup_chunk(p.x, p.y + (dy>>CHUNK_Y_S), p.z + (dz>>CHUNK_Z_S));
			if(chunk && !chunk.pending)
			{
				return chunk.data.slice(
					((dy&CHUNK_Y_MASK)<<CHUNK_X_S) +
					((dz&CHUNK_Z_MASK)<<CHUNK_XY_S) );
			}
		}
		
		return null;
	};
	
	
	if(left_buffer)		left_buffer = left_buffer.data;
	if(right_buffer)	right_buffer = right_buffer.data;		
		
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
		n200 = buf00 ? buf00[0] : 0xff;
		n201 = buf01 ? buf01[0] : 0xff;
		n202 = buf02 ? buf02[0] : 0xff;
		n210 = buf10 ? buf10[0] : 0xff;
		n211 = buf11 ? buf11[0] : 0xff;
		n212 = buf12 ? buf12[0] : 0xff;
		n220 = buf20 ? buf20[0] : 0xff;
		n221 = buf21 ? buf21[0] : 0xff;
		n222 = buf22 ? buf22[0] : 0xff;

	
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
				
				
			if(n011 != 0xff && Transparent[n011] && n011 != n111)
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
			
			if(n211 != 0xff && Transparent[n211] && n211 != n111)
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
			
			if(n101 != 0xff && Transparent[n101] && n101 != n111)
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
			
			
			if(n121 != 0xff && Transparent[n121] && n121 != n111)
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
			

			if(n110 != 0xff && Transparent[n110] && n110 != n111)
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

			if(n112 != 0xff && Transparent[n112] && n112 != n111)
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
	return [vertices, indices, tindices];
}

//Marks a chunk as dirty
function set_dirty(x, y, z)
{
	var chunk = Map.lookup_chunk(x, y, z);
	if(chunk)
	{
		vb_pending_chunks[x+":"+y+":"+z] = chunk;
	}
}


//Generates all vertex buffers
function generate_vbs()
{
	var c, chunk, vbs;
	for(c in vb_pending_chunks)
	{
		chunk = vb_pending_chunks[c];
		if(chunk instanceof Chunk)
		{
			send_vb(chunk.x, chunk.y, chunk.z, gen_vb(chunk));
		}
	}
	vb_pending_chunks = {};
}

//Sets a block
function set_block(x, y, z, b)
{
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
	if(net_pending_chunks.length == 0)
		return;

	var chunks = net_pending_chunks;
	net_pending_chunks = [];
	
	var base_chunk = net_pending_chunks[0]
	
	var bb = new BlobBuilder();
	bb.append(session_id.buffer);

	var arr = new Uint8Array(12);
	var k = 0;	
	for(var i=0; i<3; ++i)
	{
		arr[k++] = (base_chunk[i])			& 0xff;
		arr[k++] = (base_chunk[i] >> 8)		& 0xff;
		arr[k++] = (base_chunk[i] >> 16)	& 0xff;
		arr[k++] = (base_chunk[i] >> 24)	& 0xff;
		
	}
	bb.append(arr.buffer);
	
	
	//Encode chunk offsets
	for(i=0; i<chunks.length; ++i)
	{
		var delta = new Uint8Array(3);
		
		delta[0] = chunks[i].x - pchunk[0];
		delta[1] = chunks[i].y - pchunk[1];
		delta[2] = chunks[i].z - pchunk[2];
		
		bb.append(delta.buffer);
	}
	
	
	//Genereate XML Http Request
	
	var XHR = new XMLHttpRequest(

	asyncGetBinary("g", 
	function(arr)
	{
		Map.wait_chunks = false;
		arr = arr.slice(1);
	
		for(var i=0; i<chunks.length; i++)
		{
			var chunk = chunks[i], res = -1, flags;
			
			if(arr.length >= 1)
			{
				flags = arr[0];	
				res = decompress_chunk(arr, chunk.data);
			}
			
			//EOF, clear out remaining chunks
			if(res < 0)
			{
				Map.pending_chunks = chunks.slice(i).concat(Map.pending_chunks);
				return;
			}
			
			//Handle any pending writes
			chunk.pending = false;
			for(var j=0; j<chunk.pending_blocks.length; ++j)
			{
				var block = chunk.pending_blocks[j];
				chunk.set_block(block[0], block[1], block[2], block[3]);
			}
			delete chunk.pending_blocks;
			
			chunk.is_air = true;
			for(var k=0; k<CHUNK_SIZE; ++k)
			{
				if(chunk.data[k] != 0)
				{
					chunk.is_air = false;
					break;
				}
			}

			//Resize array
			arr = arr.slice(res);

			//Set dirty flags on neighboring chunks
			set_dirty(chunk.x, chunk.y, chunk.z);
			set_dirty(chunk.x-1, chunk.y, chunk.z);
			set_dirty(chunk.x+1, chunk.y, chunk.z);
			set_dirty(chunk.x, chunk.y-1, chunk.z);
			set_dirty(chunk.x, chunk.y+1, chunk.z);
			set_dirty(chunk.x, chunk.y, chunk.z-1);
			set_dirty(chunk.x, chunk.y, chunk.z+1);
		}
	}, 
	function()
	{
		net_pending_chunks = net_pending_chunks.concat(chunks);
	},
	bb.getBlob("application/octet-stream"));
}

//Fetches a chunk
function fetch_chunk(x, y, z)
{
}

//Starts the worker
function worker_start(key)
{
	session_id = key;
}


//Handles a block update
self.addEventListener('message', function(ev)
{
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
	}

}, false);

//Sends an updated set of vertex buffers to the client
function send_vb(x, y, z, vbs)
{
	postMessage({ 
		type: EV_VB_UPDATE, 
		x: x, y: y, z: z, 
		verts: new Float32Array(vbs[0]),
		ind: new Uint16Array(vbs[1]),
		tind: new Uint16Array(vbs[2])});
}

//Sends a new chunk to the client
function send_chunk(chunk)
{
	postMessage({
		type: EV_CHUNK_UPDATE,
		chunk: chunk });
}

