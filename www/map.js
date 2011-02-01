"use strict";

//The map
var Map =
{
	index			: {},	//The chunk index
	terrain_tex		: null,
	
	
	surface_index	: {},	//The surface cell index
	
	max_chunks		: 80000,	//Maximum number of chunks to load (not used yet)
	chunk_count 	: 0,		//Number of loaded chunks
	chunk_radius	: 3,		//These chunks are always fetched.
	
	show_debug		: false,
	
	vis_width		: 64,
	vis_height		: 64,
	vis_fov			: Math.PI * 3.0 / 4.0,
	vis_state		: 0,	
	vis_bounds		: [ [1, 3],
						[4, 4],
						[5, 5],
						[6, 6],
						[7, 7],
						[8, 8],
						[9, 9],
						[10, 10],
						[11, 11],
						[12, 12] ],
				
	pending_chunks	: [],	//Chunks waiting to be fetched						
	wait_chunks		: false	//Chunks are waiting
	
	
};

Map.init = function(gl)
{
	//Initialize empty chunk buffer
	for(var i=0; i<CHUNK_SIZE; ++i)
	{
		ChunkVB.prototype.empty_data[i] = 0xff;
	}

	var res = getProgram(gl, "shaders/chunk.fs", "shaders/chunk.vs");
	if(res[0] != "Ok")
	{
		return res[1];
	}
	
	//Read in return variables
	Map.chunk_fs 	 = res[1];
	Map.chunk_vs 	 = res[2];
	Map.chunk_shader = res[3];
	
	//Get attributes
	Map.chunk_shader.pos_attr = gl.getAttribLocation(Map.chunk_shader, "pos");
	if(Map.chunk_shader.pos_attr == null)
		return "Could not locate position attribute";

	Map.chunk_shader.tc_attr = gl.getAttribLocation(Map.chunk_shader, "texCoord");
	if(Map.chunk_shader.tc_attr == null)
		return "Could not locate tex coord attribute";

	Map.chunk_shader.proj_mat = gl.getUniformLocation(Map.chunk_shader, "proj");
	if(Map.chunk_shader.proj_mat == null)
		return "Could not locate projection matrix uniform";
	
	Map.chunk_shader.view_mat = gl.getUniformLocation(Map.chunk_shader, "view");
	if(Map.chunk_shader.view_mat == null)
		return "Could not locate view matrix uniform";

	Map.chunk_shader.tex_samp = gl.getUniformLocation(Map.chunk_shader, "tex");
	if(Map.chunk_shader.tex_samp == null)
		return "Could not locate sampler uniform";
		
	//Create texture
	res = getTexture(gl, "img/terrain.png");
	if(res[0] != "Ok")
	{
		return res[1];
	}
	Map.terrain_tex = res[1];
	
	//Create visibility prog
	res = getProgram(gl, "shaders/vis.fs", "shaders/vis.vs");
	if(res[0] != "Ok")
	{
		return res[1];
	}
	
	Map.vis_fs = res[1];
	Map.vis_vs = res[2];
	Map.vis_shader = res[3];
	
	//Get attributes
	Map.vis_shader.pos_attr = gl.getAttribLocation(Map.vis_shader, "pos");
	if(Map.vis_shader.pos_attr == null)
		return "Could not locate position attribute";

	Map.vis_shader.proj_mat = gl.getUniformLocation(Map.vis_shader, "proj");
	if(Map.vis_shader.proj_mat == null)
		return "Could not locate projection matrix uniform";
	
	Map.vis_shader.view_mat = gl.getUniformLocation(Map.vis_shader, "view");
	if(Map.vis_shader.view_mat == null)
		return "Could not locate view matrix uniform";
		
	Map.vis_shader.chunk_id = gl.getUniformLocation(Map.vis_shader, "chunk_id");
	if(Map.vis_shader.chunk_id == null)
		return "Could not locate chunk_id uniform";
	
	
	//Create chunk visibility frame buffer
	Map.vis_tex = gl.createTexture();
	gl.bindTexture(gl.TEXTURE_2D, Map.vis_tex);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NONE);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NONE);
	gl.texParameteri(gl.TEXTURE_2D,	gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
	gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, Map.vis_width, Map.vis_height, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
	gl.bindTexture(gl.TEXTURE_2D, null);
	
	var depth_rb = gl.createRenderbuffer();
	gl.bindRenderbuffer(gl.RENDERBUFFER, depth_rb);
	gl.renderbufferStorage(gl.RENDERBUFFER, gl.DEPTH_COMPONENT16, Map.vis_width, Map.vis_height);
	gl.bindRenderbuffer(gl.RENDERBUFFER, null);
	
	Map.vis_fbo = gl.createFramebuffer();
	gl.bindFramebuffer(gl.FRAMEBUFFER, Map.vis_fbo);
	gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, Map.vis_tex, 0);
	gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.RENDERBUFFER, depth_rb);
	
	if(!gl.isFramebuffer(Map.vis_fbo))
	{
		return "Could not create visibility frame buffer";
	}
	
	//Clear out FBO
	gl.viewport(0, 0, Map.vis_width, Map.vis_height);
	gl.clearColor(0, 0, 0, 1);
	gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
	gl.bindFramebuffer(gl.FRAMEBUFFER, null);
	
	//Initialize pixel array
	Map.vis_data = new Uint8Array(4 * Map.vis_width * Map.vis_height);
	
	//Create box array
    var vertices = new Float32Array( [
    	0,			CHUNK_Y,	0,		1,
    	CHUNK_X,	CHUNK_Y,	0,		1,
    	0,			0,			0,		1,
    	CHUNK_X,	0,			0,		1,
    	0,			CHUNK_Y,	CHUNK_Z,1,
    	CHUNK_X,	CHUNK_Y,	CHUNK_Z,1,
    	0,			0,			CHUNK_Z,1,
    	CHUNK_X,	0,			CHUNK_Z,1]);
    
    //FIXME: Convert this to a triangle strip
    var indices = new Uint16Array( [
			0,4,2, 2,4,6,		//-x
			1,7,5, 1,3,7,		//+x
    
			2,6,3, 3,6,7,		//-y
			0,1,4, 1,5,4,		//+y

			0,3,1, 0,2,3,		//-z
			4,5,6, 5,7,6		//+z
			]);
    
    Map.box_vb = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, Map.box_vb);
	gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);
	gl.bindBuffer(gl.ARRAY_BUFFER, null);
	
	Map.box_ib = gl.createBuffer();
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, Map.box_ib);
	gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, indices, gl.STATIC_DRAW);
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, null);
    
    Map.box_elements = indices.length;
    
    //Create debug shader
	var res = getProgram(gl, "shaders/simple.fs", "shaders/simple.vs");
	if(res[0] != "Ok")
	{
		return res[1];
	}
	
	//Read in return variables
	Map.simple_fs 	 = res[1];
	Map.simple_vs 	 = res[2];
	Map.simple_shader = res[3];
	
	//Get attributes
	Map.simple_shader.pos_attr = gl.getAttribLocation(Map.simple_shader, "pos");
	if(Map.simple_shader.pos_attr == null)
		return "Could not locate position attribute";

	Map.simple_shader.tc_attr = gl.getAttribLocation(Map.simple_shader, "texCoord");
	if(Map.simple_shader.tc_attr == null)
		return "Could not locate tex coord attribute";

	Map.simple_shader.tex_samp = gl.getUniformLocation(Map.simple_shader, "tex");
	if(Map.simple_shader.tex_samp == null)
		return "Could not locate sampler uniform";
		
	
	var debug_verts = new Float32Array([
		0, 0, 0,
		0, 1, 0,
		1, 1, 0,
		1, 0, 0]);

	var debug_tc = new Float32Array([
		0, 0,
		0, 1,
		1, 1,
		1, 0] );
		
	var debug_ind = new Uint16Array([0, 1, 2, 0, 2, 3]);
	
	Map.debug_vb = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, Map.debug_vb);
	gl.bufferData(gl.ARRAY_BUFFER, debug_verts, gl.STATIC_DRAW);
	
	Map.debug_tb = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, Map.debug_tb);
	gl.bufferData(gl.ARRAY_BUFFER, debug_tc, gl.STATIC_DRAW);
	
	gl.bindBuffer(gl.ARRAY_BUFFER, null);
	
	Map.debug_ib = gl.createBuffer();
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, Map.debug_ib);
	gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, debug_ind, gl.STATIC_DRAW);
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, null);
    
	
	return "Ok";
}

//Returns the height of the column at the location x,z
Map.get_height = function(x, z)
{
	var cell = Map.surface_index[(x>>CHUNK_X_S) + ":" + (z>>CHUNK_Z_S)];
	if(cell)
		return cell[(x&CHUNK_X_MASK) + ((z&CHUNK_Z_MASK)<<CHUNK_X_S)];
	return 0;
}

Map.get_height_cell = function(x, z)
{
	var cell_idx = x + ":" + z;
	var cell = Map.surface_index[cell_idx];

	if(!cell)
	{
		cell = new Uint32Array(CHUNK_X * CHUNK_Z);
		Map.surface_index[cell_idx] = cell;
	}

	return cell;
}

//Updates the surface based on the data in a new chunk
Map.update_height = function(chunk)
{
	var cell = Map.get_height_cell(chunk.x, chunk.z);
	
	var idx = 0, should_drill = false, update_depth = chunk.y;
	for(var z=0; z<CHUNK_Z; ++z)
	for(var x=0; x<CHUNK_X; ++x)
	{
		var pheight = cell[idx];
		
		for(var y=CHUNK_Y-1; y>=0; --y)
		{
			if(!Transparent[chunk.data[x + (y<<CHUNK_X_S) + (z<<CHUNK_XY_S)]])
			{
				var nheight = y + (chunk.y<<CHUNK_Y_S);
				
				if(pheight < nheight)
				{		
					cell[idx] = nheight;
					if(pheight > 0)
						update_depth = Math.min(update_depth, (pheight>>CHUNK_Y_S));
				}
				break;
			}
		}
		
		if(cell[idx] == 0)
			should_drill = true;
		++idx;
	}
	
	
	//Update chunks below this chunk, which may have changed
	for(var y=update_depth; y<=chunk.y; ++y)
	for(var dx=chunk.x-1; dx<=chunk.x+1; ++dx)
	for(var dz=chunk.z-1; dz<=chunk.z+1; ++dz)
	{
		var Q = Map.lookup_chunk(dx, y, dz);
		if(Q)
			Q.vb.set_dirty();
	}
	
	//Need to dig down to the bottom to find correct surface height value
	if(should_drill)
	{
		Map.fetch_chunk(chunk.x, chunk.y-1, chunk.z);
	}
}

//Gets bounds for the surface (in y) for the given cell
Map.surface_bounds = function(x, z)
{
	var cell = Map.get_height_cell(x, z), 
		ymin, ymax, idx, y;
		
	ymin = cell[0];
	ymax = ymin;
		
	for(idx=1; idx<CHUNK_X*CHUNK_Z; ++idx)
	{
		ymin = Math.min(ymin, y);
		ymax = Math.max(ymax, y);
	}
	
	return [ymin, ymax];	
}


//Used for visibility testing
Map.draw_box = function(gl, cx, cy, cz)
{
	var pos = new Float32Array([1, 0, 0, 0,
								0, 1, 0, 0,
								0, 0, 1, 0,
								(cx+0.5)*CHUNK_X, 
								(cy+0.5)*CHUNK_Y, 
								(cz+0.5)*CHUNK_Z, 1]);
	
	//Set uniform
	gl.uniformMatrix4fv(Map.vis_shader.view_mat, false, pos);
	gl.uniform4f(Map.vis_shader.chunk_id, (cx&0xff)/255.0, (cy&0xff)/255.0, (cz&0xff)/255.0, 1.0);
	
	//Draw the cube
	if(!Map.just_drew_box)
	{
		gl.bindBuffer(gl.ARRAY_BUFFER, Map.box_vb);
		gl.vertexAttribPointer(Map.vis_shader.pos_attr, 4, gl.FLOAT, false, 0, 0);
		gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, Map.box_ib);
	}
	gl.drawElements(gl.TRIANGLES, Map.box_elements, gl.UNSIGNED_SHORT, 0);
}

Map.visibility_query = function(gl, camera)
{
	if(Map.vis_state == Map.vis_bounds.length+2)
	{
		//Process data to find visible chunks
		for(var i=0; i<Map.vis_data.length; i+=4)
		{
			if(i > 0 && 
				Map.vis_data[i]   == Map.vis_data[i-4] &&
				Map.vis_data[i+1] == Map.vis_data[i-3] &&
				Map.vis_data[i+2] == Map.vis_data[i-2] )
			{
				continue;
			}
	
			//Issue the fetch request
			var bx = Map.vis_base_chunk[0] + ((Map.vis_data[i]<<24)>>24),
				by = Map.vis_base_chunk[1] + ((Map.vis_data[i+1]<<24)>>24),
				bz = Map.vis_base_chunk[2] + ((Map.vis_data[i+2]<<24)>>24);	
			for(var dx=bx-1; dx<=bx+1; ++dx)
			for(var dy=by; dy<=by+1; ++dy)
			for(var dz=bz-1; dz<=bz+1; ++dz)
			{
				Map.fetch_chunk(dx, dy, dz);
			}
		}
		
		Map.vis_state = 0;
		return;
	}

	//Start by binding fbo
	gl.bindFramebuffer(gl.FRAMEBUFFER, Map.vis_fbo);
	gl.viewport(0, 0, Map.vis_width, Map.vis_height);
	
	if(Map.vis_state == Map.vis_bounds.length+1)
	{
		//Read pixels
		gl.readPixels(0, 0, Map.vis_width, Map.vis_height, gl.RGBA, gl.UNSIGNED_BYTE, Map.vis_data);	
	}
	else if(Map.vis_state == 0)
	{
		//Initialize background
		gl.clearColor(0, 0, 0, 1);
		gl.clear(gl.DEPTH_BUFFER_BIT | gl.COLOR_BUFFER_BIT);
	
		//Get camera
		Map.vis_camera = Game.camera_matrix(Map.vis_width, Map.vis_height, Map.vis_fov);
		Map.vis_base_chunk = Player.chunk();
	}
	else
	{
		//Set up shader program
		gl.useProgram(Map.vis_shader);
		gl.enableVertexAttribArray(Map.vis_shader.pos_attr);
		gl.uniformMatrix4fv(Map.vis_shader.proj_mat, false, Map.vis_camera);
	
		//Set state flags
		gl.disable(gl.BLEND);
		gl.blendFunc(gl.ONE, gl.ZERO);
		gl.enable(gl.DEPTH_TEST);
		gl.frontFace(gl.CW);
		gl.enable(gl.CULL_FACE);

		Map.vis_just_drew_box = false;
	
		var query_chunk = function(cx, cy, cz)
		{
			var c = Map.lookup_chunk(
				Map.vis_base_chunk[0] + cx, 
				Map.vis_base_chunk[1] + cy, 
				Map.vis_base_chunk[2] + cz);
	
			if(!c || c.pending)
			{
				Map.draw_box(gl, cx, cy, cz);
				Map.vis_just_drew_box = true;
			}
			else
			{
				Map.vis_just_drew_box = false;
				c.draw_vis(gl, Map.vis_shader, Map.vis_camera);
			}
		}

	
		if(Map.vis_state == 1)
		{
			query_chunk(0, 0, 0);
		}
	
		//Render all chunks to front-to-back
		var rlo = Map.vis_bounds[Map.vis_state-1][0],
			rhi = Map.vis_bounds[Map.vis_state-1][1];
		
		for(var r=rlo; r<=rhi; ++r)
		{
			for(var a=-r; a<=r; ++a)
			for(var b=-r; b<=r; ++b)
			{
				query_chunk( r, a, b);
				query_chunk(-r, a, b);
				query_chunk(a, r, b);
				query_chunk(a, -r, b);
				query_chunk(a, b, r);
				query_chunk(a, b, -r);
			}
		}
	}
	
	++Map.vis_state;
	gl.bindFramebuffer(gl.FRAMEBUFFER, null);
}

//Updates the cache of chunks
Map.update_cache = function()
{
	//Need to grab all the chunks in the viewable cube around the player
	var c = Player.chunk();
	
	for(var i=c[0] - Map.chunk_radius; i<=c[0] + Map.chunk_radius; i++)
	for(var j=c[1] - Map.chunk_radius; j<=c[1] + Map.chunk_radius; j++)
	for(var k=c[2] - Map.chunk_radius; k<=c[2] + Map.chunk_radius; k++)
	{
		Map.fetch_chunk(i, j, k);
	}
	
	/*
	if(!Map.wait_chunks && Game.local_ticks % 2 == 1)
	{
		Map.visibility_query(Game.gl);
	}
	*/
	
	//If we are over the chunk count, remove old chunks
	if(Map.chunk_count > Map.max_chunks)
	{
		//TODO: Purge old chunks
	}

	//Grab all pending chunks
	Map.grab_chunks();	
}

//Draws the map
// Input:
//  gl - the open gl rendering context
//  camera - the current camera matrix
Map.draw = function(gl, camera)
{
	var c, chunk;
	gl.useProgram(Map.chunk_shader);
		
	//Enable attributes
	gl.enableVertexAttribArray(Map.chunk_shader.pos_attr);
	gl.enableVertexAttribArray(Map.chunk_shader.tc_attr);
	
	//Load matrix uniforms
	gl.uniformMatrix4fv(Map.chunk_shader.proj_mat, false, camera);

	//Set texture index
	gl.activeTexture(gl.TEXTURE0);
	gl.bindTexture(gl.TEXTURE_2D, Map.terrain_tex);
	gl.uniform1i(Map.chunk_shader.tex_samp, 0);
	
	//Draw all the chunks
	for(c in Map.index)
	{
		var chunk = Map.index[c];
		if(chunk instanceof Chunk)
			chunk.draw(gl, Map.chunk_shader, camera, false);
	}

	gl.enable(gl.BLEND);
	gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
	gl.depthMask(0);
	for(c in Map.index)
	{
		var chunk = Map.index[c];
		if(chunk instanceof Chunk)
			chunk.draw(gl, Map.chunk_shader, camera, true);
	}
	gl.depthMask(1);
	
	if(Map.show_debug)
	{
		gl.disable(gl.BLEND);
		gl.disable(gl.DEPTH_TEST);
		gl.enable(gl.TEXTURE_2D);
		
		//Enable attributes
		gl.useProgram(Map.simple_shader);
		gl.enableVertexAttribArray(Map.simple_shader.pos_attr);
		gl.enableVertexAttribArray(Map.simple_shader.tc_attr);

		//Set texture index
		gl.activeTexture(gl.TEXTURE0);
		gl.bindTexture(gl.TEXTURE_2D, Map.vis_tex);
		gl.generateMipmap(gl.TEXTURE_2D);
		gl.uniform1i(Map.simple_shader.tex_samp, 0);
		
		gl.bindBuffer(gl.ARRAY_BUFFER, Map.debug_vb);
		gl.vertexAttribPointer(Map.simple_shader.pos_attr, 3, gl.FLOAT, false, 0, 0);
	
		gl.bindBuffer(gl.ARRAY_BUFFER, Map.debug_tb);
		gl.vertexAttribPointer(Map.simple_shader.tc_attr, 2, gl.FLOAT, false, 0, 0);
		
		gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, Map.debug_ib);
		gl.drawElements(gl.TRIANGLES, 6, gl.UNSIGNED_SHORT, 0);
	}	
}

//Decodes a run-length encoded chunk
Map.decompress_chunk = function(arr, data)
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

//Downloads a chunk from the server
Map.fetch_chunk = function(x, y, z)
{
	//If chunk is already stored, don't get it
	if(Map.lookup_chunk(x,y,z))
		return;

	Map.wait_chunks = true;

	//Add new chunk, though leave it empty
	var chunk = new Chunk(x, y, z, new Uint8Array(CHUNK_SIZE));
	Map.add_chunk(chunk);
	Map.pending_chunks.push(chunk);
}


Map.grab_chunks = function()
{
	if(Map.pending_chunks.length == 0)
		return;

	var pchunk = Player.chunk();
	var chunks = Map.pending_chunks;
	Map.pending_chunks = [];
	
	var bb = new BlobBuilder();
	bb.append(Session.get_session_id_arr().buffer);

	var arr = new Uint8Array(12);
	var k = 0;	
	for(var i=0; i<3; ++i)
	{
		arr[k++] = (pchunk[i])			& 0xff;
		arr[k++] = (pchunk[i] >> 8)		& 0xff;
		arr[k++] = (pchunk[i] >> 16)	& 0xff;
		arr[k++] = (pchunk[i] >> 24)	& 0xff;
		
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
				res = Map.decompress_chunk(arr, chunk.data);
			}
			
			
			//EOF, clear out remaining chunks
			if(res < 0)
			{
				Map.pending_chunks = chunks.slice(i).concat(Map.pending_chunks);
				return;
			}
			
			//Update height field
			//Map.update_height(chunk);
			
			//Update state flags
			chunk.vb.set_dirty();
			chunk.pending = false;
			for(var j=0; j<chunk.pending_blocks.length; ++j)
			{
				var block = chunk.pending_blocks[j];
				chunk.set_block(block[0], block[1], block[2], block[3]);
			}
			delete chunk.pending_blocks;
			
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

			if(!chunk.is_air)
			{
				var c = Map.lookup_chunk(chunk.x-1, chunk.y, chunk.z);
				if(c)	c.vb.set_dirty();
			
				c = Map.lookup_chunk(chunk.x+1, chunk.y, chunk.z);
				if(c)	c.vb.set_dirty();
			
				c = Map.lookup_chunk(chunk.x, chunk.y-1, chunk.z);
				if(c)	c.vb.set_dirty();

				c = Map.lookup_chunk(chunk.x, chunk.y+1, chunk.z);
				if(c)	c.vb.set_dirty();

				c = Map.lookup_chunk(chunk.x, chunk.y, chunk.z-1);
				if(c)	c.vb.set_dirty();

				c = Map.lookup_chunk(chunk.x, chunk.y, chunk.z+1);
				if(c)	c.vb.set_dirty();
			}
		}
	}, 
	function()
	{
		for(var j=0; j<chunks.length; j++)
		{
			Map.remove_chunk(chunks[j]);
		}
	},
	bb.getBlob("application/octet-stream"));
}

//Converts a block index into an indexable string
Map.index2str = function(x, y, z)
{
	return x + ":" + y + ":" + z;
}

//Adds a chunk to the list
Map.add_chunk = function(chunk)
{
	var str = Map.index2str(chunk.x, chunk.y, chunk.z);
	Map.index[str] = chunk;
	++Map.chunk_count;
}

//Hash look up in map
Map.lookup_chunk = function(x, y, z)
{
	var str = Map.index2str(x, y, z);
	return Map.index[str];
}

//Just removes the chunk from the list
Map.remove_chunk = function(chunk)
{
	if(Map.lookup_chunk(chunk.x, chunk.y, chunk.z))
	{
		--Map.chunk_count;
		chunk.release(Game.gl);
		var str = Map.index2str(chunk.x, chunk.y, chunk.z);
		delete Map.index[str];
	}
}

//Returns the block type for the give position
Map.get_block = function(x, y, z)
{
	var cx = (x >> CHUNK_X_S), 
		cy = (y >> CHUNK_Y_S), 
		cz = (z >> CHUNK_Z_S);
	var c = Map.lookup_chunk(cx, cy, cz);		
	if(!c)
		return -1;
	
	var bx = (x & CHUNK_X_MASK), 
		by = (y & CHUNK_Y_MASK), 
		bz = (z & CHUNK_Z_MASK);
	return c.data[bx + (by << CHUNK_X_S) + (bz << CHUNK_XY_S)];
}

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

//Traces a ray into the map, returns the index of the block hit, its type and the hit normal
// Meant for UI actions, not particularly fast over long distances
Map.trace_ray = function(
	ox, oy, oz,
	dx, dy, dz,
	max_d)
{
	//Normalize D
	var ds = Math.sqrt(dx*dx + dy*dy + dz*dz);
	dx /= ds;
	dy /= ds;
	dz /= ds;
	
	//Step block-by-bloc along raywc
	var t = 0.0;
	
	var norm = [0, 0, 0];
	
	while(t <= max_d)
	{
		var b = Map.get_block(Math.round(ox), Math.round(oy), Math.round(oz));
		if(b != 0)
			return [ox, oy, oz, b, norm[0], norm[1], norm[2]];
			
		var step = 0.5;
		
		var fx = ox - Math.floor(ox);
		var fy = oy - Math.floor(oy);
		var fz = oz - Math.floor(oz);
				
		if(dx < -0.0001)
		{
			if(fx < 0.0001)
				fx = 1.0;
		
			var s = -fx/dx;
			
			if(s < step)
			{
				norm = [1, 0, 0];
				step = s;
			}
		}
		if(dx > 0.0001)
		{
			var s = (1.0-fx)/dx;
			if(s < step)
			{
				norm = [-1, 0, 0];
				step = s;
			}
		}
		
		if(dy < -0.0001)
		{
			if(fy < 0.0001)
				fy = 1.0;

			var s = -fy/dy;
			if(s < step)
			{
				norm = [0, 1, 0];
				step = s;
			}
		}
		if(dy > 0.0001)
		{
			var s = (1.0-fy)/dy;
			if(s < step)
			{
				norm = [0, -1, 0];
				step = s;
			}
		}

		if(dz < -0.0001)
		{
			if(fz < 0.0001)
				fz = 1.0;
		
			var s = -fz/dz;
			if(s < step)
			{
				norm = [0, 0, 1];
				step = s;
			}
		}
		if(dz > 0.0001)
		{
			var s = (1.0-fz)/dz;
			if(s < step)
			{
				norm = [0, 0, -1];
				step = s;
			}
		}
		
		t += step;
		ox += dx * step;
		oy += dy * step;
		oz += dz * step;
	}
	
	return [];
}

