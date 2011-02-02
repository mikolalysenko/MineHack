
//Draws a chunk
Chunk.prototype.draw = function(gl, cam, shader, transp)
{
	//TODO: Check if we have a vertex buffer
	if(this.pending || !this.in_frustum(cam))
		return;
		
	var c = Player.chunk();
	var pos = new Float32Array([1, 0, 0, 0,
								0, 1, 0, 0,
								0, 0, 1, 0,
								(this.x-c[0])*CHUNK_X, 
								(this.y-c[1])*CHUNK_Y, 
								(this.z-c[2])*CHUNK_Z, 1]);
	
	gl.uniformMatrix4fv(shader.view_mat, false, pos);
	
	//Bind buffers
	gl.bindBuffer(gl.ARRAY_BUFFER, this.vb);
	if(shader.pos_attr)
		gl.vertexAttribPointer(shader.pos_attr,	4, gl.FLOAT, false, 32, 0);	
	if(shader.tc_attr)
		gl.vertexAttribPointer(shader.tc_attr, 	4, gl.FLOAT, false, 32, 16);

	if(transp)
	{
		gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.tib);
		gl.drawElements(gl.TRIANGLES, this.num_transparent_elements, gl.UNSIGNED_SHORT, 0);
	}
	else
	{
		gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.ib);
		gl.drawElements(gl.TRIANGLES, this.num_elements, gl.UNSIGNED_SHORT, 0);
	}
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
	
	//FIXME: Do a frustum test here maybe?
	
	//Draw the cube
	if(!Map.just_drew_box)
	{
		gl.bindBuffer(gl.ARRAY_BUFFER, Map.box_vb);
		gl.vertexAttribPointer(Map.vis_shader.pos_attr, 4, gl.FLOAT, false, 0, 0);
		gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, Map.box_ib);
	}
	gl.drawElements(gl.TRIANGLES, Map.box_elements, gl.UNSIGNED_SHORT, 0);
}


//Initialize the map
Map.init = function(gl)
{
	//Initialize chunk shader
	var res = getProgram(gl, "shaders/chunk.fs", "shaders/chunk.vs");
	if(res[0] != "Ok")
	{
		return res[1];
	}
	
	Map.chunk_fs 	 = res[1];
	Map.chunk_vs 	 = res[2];
	Map.chunk_shader = res[3];
	
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
		
	//Create terrain texture
	res = getTexture(gl, "img/terrain.png");
	if(res[0] != "Ok")
	{
		return res[1];
	}
	Map.terrain_tex = res[1];
	
	//Initialize visibility shader
	res = getProgram(gl, "shaders/vis.fs", "shaders/vis.vs");
	if(res[0] != "Ok")
	{
		return res[1];
	}
	
	Map.vis_fs = res[1];
	Map.vis_vs = res[2];
	Map.vis_shader = res[3];
	
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
	
	Map.simple_fs 	 = res[1];
	Map.simple_vs 	 = res[2];
	Map.simple_shader = res[3];
	
	Map.simple_shader.pos_attr = gl.getAttribLocation(Map.simple_shader, "pos");
	if(Map.simple_shader.pos_attr == null)
		return "Could not locate position attribute";

	Map.simple_shader.tc_attr = gl.getAttribLocation(Map.simple_shader, "texCoord");
	if(Map.simple_shader.tc_attr == null)
		return "Could not locate tex coord attribute";

	Map.simple_shader.tex_samp = gl.getUniformLocation(Map.simple_shader, "tex");
	if(Map.simple_shader.tex_samp == null)
		return "Could not locate sampler uniform";
		
	//Create debug buffers
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

//Does a pass of the visibility query
Map.visibility_query = function()
{
	var gl = Game.gl;
	
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
	
	//Draw regular chunks
	for(c in Map.index)
	{
		chunk = Map.index[c];
		if(chunk instanceof Chunk)
			chunk.draw(gl, Map.chunk_shader, camera, false);
	}

	//Draw transparent chunks
	gl.enable(gl.BLEND);
	gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
	gl.depthMask(0);
	for(c in Map.index)
	{
		chunk = Map.index[c];
		if(chunk instanceof Chunk)
			chunk.draw(gl, Map.chunk_shader, camera, true);
	}
	gl.depthMask(1);
	
	//Optional: draw debug information for visibility query
	if(Map.show_debug)
	{
		gl.disable(gl.BLEND);
		gl.disable(gl.DEPTH_TEST);
		gl.enable(gl.TEXTURE_2D);
		
		gl.useProgram(Map.simple_shader);
		gl.enableVertexAttribArray(Map.simple_shader.pos_attr);
		gl.enableVertexAttribArray(Map.simple_shader.tc_attr);

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


//Updates the cache of chunks
Map.update_cache = function()
{
	//Need to grab all the chunks in the viewable cube around the player
	var c = Player.chunk();

	//TODO: Post an update event to the chunk worker
	
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
	
	//TODO: Purge old chunks
}


//Downloads a chunk from the server
Map.fetch_chunk = function(x, y, z)
{
	//If chunk is already stored, don't get it
	if(Map.lookup_chunk(x,y,z))
		return;

	var chunk = Map.add_chunk(x, y, z);
	chunk.pending = true;
	
	Map.vb_worker.postMessage({type: EV_FETCH_CHUNK,
		'x':x, 'y':y, 'z':z});
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
		
	//Post message to worker
	Map.vb_worker.postMessage({type: EV_SET_BLOCK,
		'x':x, 'y':y, 'z':z, 'b':b});
		
	var bx = (x & CHUNK_X_MASK), 
		by = (y & CHUNK_Y_MASK), 
		bz = (z & CHUNK_Z_MASK);
	return c.set_block(bx, by, bz, b);
}

//Updates the vertex buffer for a chunk
Map.update_vb = function(x, y, z, verts, ind, tind)
{
	var chunk = Map.lookup_chunk(x, y, z);
	
	if(!chunk)
	{
		alert("Updating vertex buffer for unloaded chunk?");
		return;
	}

	if(chunk.pending)
	{
		chunk.pending = false;
	
		//Create buffers
	}
	else
	{
		//Update in place
	}

}

//Updates the chunk data
Map.update_chunk = function(x, y, z, data)
{
}

//Initialize the web worker
Map.init_worker = function()
{
	Map.vb_worker = new Worker('chunk_worker.js');

	
	//Set event handlers
	Map.vb_worker.addEventListener('message', function(ev)
	{
		switch(ev.data.type)
		{
			case EV_VB_UPDATE:
				Map.update_vb(
					ev.data.x, ev.data.y, ev.data.z, 
					ev.data.verts, 
					ev.data.ind, 
					ev.data.tind);
			break;
			
			case EV_CHUNK_UPDATE:
				Map.update_chunk(
					ev.data.x, ev.data.y, ev.data.z, 
					ev.data.data);
			break;
		}
	}, false);

	//Start the worker	
	Map.vb_worker.postMessage({ type: EV_START, key: Session.get_session_id_arr() });
}
