"use strict";


//Draws a chunk
//
// Convention:
//
//	Attrib 0 = position
//	Attrib 1 = normal
//	Attrib 2 = tex coord
//	Attrib 3 = light map value
//
Chunk.prototype.draw = function(gl, cam, base_chunk, shader, transp)
{
	if(	this.pending || 
		(transp && this.num_transparent_elements == 0) ||
		(!transp && this.num_elements == 0) ||
		!frustum_test(cam, this.x - base_chunk[0], this.y - base_chunk[1], this.z - base_chunk[2]) )
		return;
		

	gl.uniform3f(shader.chunk_offset, 
		(this.x-base_chunk[0])<<CHUNK_X_S, 
		(this.y-base_chunk[1])<<CHUNK_Y_S, 
		(this.z-base_chunk[2])<<CHUNK_Z_S);
	
	//Bind buffers
	gl.bindBuffer(gl.ARRAY_BUFFER, this.vb);
	if('pos_attr' in shader)
		gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 48, 0);	
	if('tc_attr' in shader)
		gl.vertexAttribPointer(1, 2, gl.FLOAT, false, 48, 12);
	if('norm_attr' in shader)
		gl.vertexAttribPointer(2, 3, gl.FLOAT, false, 48, 20);
	if('light_attr' in shader)
		gl.vertexAttribPointer(3, 3, gl.FLOAT, false, 48, 32);

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
Map.draw_box = function(gl, shader, cx, cy, cz)
{
	if(!frustum_test(Map.vis_camera, cx, cy, cz))
		return;

	//Set uniform
	gl.uniform3f(shader.chunk_offset, 
		(cx+0.5)*CHUNK_X, 
		(cy+0.5)*CHUNK_Y, 
		(cz+0.5)*CHUNK_Z);

	//Draw the cube
	if(!Map.vis_just_drew_box)
	{
		gl.bindBuffer(gl.ARRAY_BUFFER, Map.box_vb);
		gl.vertexAttribPointer(shader.pos_attr, 4, gl.FLOAT, false, 0, 0);
		gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, Map.box_ib);
		Map.vis_just_drew_box = true;
	}
	gl.drawElements(gl.TRIANGLES, Map.box_elements, gl.UNSIGNED_SHORT, 0);
}


//Initialize the map
Map.init = function(gl)
{
	//Initialize chunk shader
	var res = getProgram(gl, "shaders/chunk.fs", "shaders/chunk.vs"), i;
	if(res[0] != "Ok")
	{
		return res[1];
	}
	
	Map.chunk_fs 	 = res[1];
	Map.chunk_vs 	 = res[2];
	Map.chunk_shader = res[3];
	
	//Bind attributes
	Map.chunk_shader.pos_attr		= 0;
	Map.chunk_shader.tc_attr		= 1;
	Map.chunk_shader.norm_attr		= 2;
	Map.chunk_shader.light_attr		= 3;
	gl.bindAttribLocation(Map.chunk_shader, Map.chunk_shader.pos_attr, "pos");
	gl.bindAttribLocation(Map.chunk_shader, Map.chunk_shader.tc_attr, "texCoord");
	gl.bindAttribLocation(Map.chunk_shader, Map.chunk_shader.norm_attr, "normal");
	gl.bindAttribLocation(Map.chunk_shader, Map.chunk_shader.light_attr, "lightColor");
	
	Map.chunk_shader.proj_mat = gl.getUniformLocation(Map.chunk_shader, "proj");
	if(Map.chunk_shader.proj_mat == null)
		return "Could not locate projection matrix uniform";
	
	Map.chunk_shader.chunk_offset = gl.getUniformLocation(Map.chunk_shader, "chunk_offset");
	if(Map.chunk_shader.chunk_offset == null)
		return "Could not locate chunk offset uniform";

	Map.chunk_shader.tex_samp = gl.getUniformLocation(Map.chunk_shader, "tex");
	if(Map.chunk_shader.tex_samp == null)
		return "Could not locate sampler uniform";
		
	Map.chunk_shader.sun_dir = gl.getUniformLocation(Map.chunk_shader, "sun_dir");
	if(Map.chunk_shader.sun_dir == null)
		return "Could not locate sunlight direction uniform";

	Map.chunk_shader.sun_color = gl.getUniformLocation(Map.chunk_shader, "sun_color");
	if(Map.chunk_shader.sun_color == null)
		return "Could not locate sunlight color uniform";
		
	Map.chunk_shader.light_mat = gl.getUniformLocation(Map.chunk_shader, "shadow");
	Map.chunk_shader.shadow_tex = gl.getUniformLocation(Map.chunk_shader, "shadow_tex");

	Map.chunk_shader.shadow_fudge_factor = gl.getUniformLocation(Map.chunk_shader, "shadow_fudge_factor");
	if(Map.chunk_shader.shadow_fudge_factor == null)
		return "Could not locate shadow fudge uniform";
		
		
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
	
	/*
	Map.vis_shader.pos_attr = gl.getAttribLocation(Map.vis_shader, "pos");
	if(Map.vis_shader.pos_attr == null)
		return "Could not locate position attribute";
	*/
	
	Map.vis_shader.pos_attr = 0;
	gl.bindAttribLocation(Map.vis_shader, Map.vis_shader.pos_attr, "pos");
	

	Map.vis_shader.proj_mat = gl.getUniformLocation(Map.vis_shader, "proj");
	if(Map.vis_shader.proj_mat == null)
		return "Could not locate projection matrix uniform";

		
	Map.vis_shader.chunk_offset = gl.getUniformLocation(Map.vis_shader, "chunk_offset");
	if(Map.vis_shader.chunk_offset == null)
		return "Could not locate chunk_offset uniform";
	
		
	Map.vis_shader.chunk_id = gl.getUniformLocation(Map.vis_shader, "chunk_id");
	if(Map.vis_shader.chunk_id == null)
		return "Could not locate chunk_id uniform";
	
	//Create chunk visibility frame buffer
	Map.vis_tex = gl.createTexture();
	gl.bindTexture(gl.TEXTURE_2D, Map.vis_tex);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
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
    
	//Start worker
	Map.init_worker();
	
	return "Ok";
}

//Does a pass of the visibility query
Map.visibility_query = function()
{
	var gl = Game.gl;

	//Start by binding fbo
	gl.bindFramebuffer(gl.FRAMEBUFFER, Map.vis_fbo);
	gl.viewport(0, 0, Map.vis_width, Map.vis_height);
	
	if(Map.vis_state == Map.vis_bounds.length)
	{
		//Read pixels
		gl.readPixels(0, 0, Map.vis_width, Map.vis_height, gl.RGBA, gl.UNSIGNED_BYTE, Map.vis_data);	
		
		
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
			
			Map.fetch_chunk(bx, by, bz);
		}
		
		Map.vis_state = 0;
		Map.vis_angle = (Map.vis_angle + 1) % 7;
		return;
	}
	else
	{
		if(Map.vis_state == 0)
		{
			//Initialize background
			gl.clearColor(0, 0, 0, 1);
			gl.clear(gl.DEPTH_BUFFER_BIT | gl.COLOR_BUFFER_BIT);
	
			//Get camera
			if(Map.vis_angle == 6)
			{
				Map.vis_camera = Game.camera_matrix(Map.vis_width, Map.vis_height, Map.vis_fov, 1024, 1);
			}
			else
			{
				//Need to shift by player chunk in order to avoid numerical problems
				var c = Player.chunk();	
				c[0] *= CHUNK_X;
				c[1] *= CHUNK_Y;
				c[2] *= CHUNK_Z;
	
				var trans = [
					1, 0, 0, 0,
					0, 1, 0, 0,
					0, 0, 1, 0,
					c[0]-Player.entity.x, c[1]-Player.entity.y, c[2]-Player.entity.z, 1];
				
				var rot = [
					0, 0, 0, 0,
					0, 0, 0, 0,
					0, 0, 0, 0,
					0, 0, 0, 1];
				
			
				var k = Math.floor(Map.vis_angle / 2),
					s = (Map.vis_angle % 2)*2 - 1;	
			
			
				rot[k] = s;
				rot[4+((k+1)%3)] = 1;
				rot[8+((k+2)%3)] = s;
			
				Map.vis_camera = mmult(
					Game.proj_matrix(Map.vis_width, Map.vis_height, Map.vis_fov, 1024, 1),
					mmult(rot, trans));
			}
			Map.vis_base_chunk = Player.chunk();
		}
		
		//Set up shader program
		gl.useProgram(Map.vis_shader);
		gl.enableVertexAttribArray(Map.vis_shader.pos_attr);
		gl.uniformMatrix4fv(Map.vis_shader.proj_mat, false, Map.vis_camera);
	
		//Set state flags
		gl.disable(gl.BLEND);
		gl.enable(gl.DEPTH_TEST);
		gl.frontFace(gl.CW);
		gl.cullFace(gl.BACK);
		gl.enable(gl.CULL_FACE);

		Map.vis_just_drew_box = false;
	
		function query_chunk(cx, cy, cz)
		{
			var c = Map.lookup_chunk(
				Map.vis_base_chunk[0] + cx, 
				Map.vis_base_chunk[1] + cy, 
				Map.vis_base_chunk[2] + cz);
			
			if(c)
			{
				gl.uniform4f(Map.vis_shader.chunk_id, 1, 1, 1, 1);
			}
			else
			{
				gl.uniform4f(Map.vis_shader.chunk_id, 
					(cx&0xff)/255.0, 
					(cy&0xff)/255.0, 
					(cz&0xff)/255.0, 1.0);
			}
	
			if(!c || c.pending)
			{
				Map.draw_box(gl, Map.vis_shader, cx, cy, cz);
			}
			else
			{
				Map.vis_just_drew_box = false;
				c.draw(gl, Map.vis_camera, Map.vis_base_chunk, Map.vis_shader, false);
			}
		}

		if(Map.vis_state == 1)
		{
			query_chunk(0, 0, 0);
		}
	
		//Render all chunks to front-to-back
		var rlo = Map.vis_bounds[Map.vis_state][0],
			rhi = Map.vis_bounds[Map.vis_state][1];
		
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
Map.draw = function(gl)
{
	var c, chunk, base_chunk = Player.chunk(), 
		sun_dir = Sky.get_sun_dir(), 
		sun_color = Sky.get_sun_color(),
		i, camera;
		
	gl.useProgram(Map.chunk_shader);
		
	//Enable attributes
	gl.enableVertexAttribArray(Map.chunk_shader.pos_attr);
	gl.enableVertexAttribArray(Map.chunk_shader.tc_attr);
	gl.enableVertexAttribArray(Map.chunk_shader.norm_attr);
	gl.enableVertexAttribArray(Map.chunk_shader.light_attr);


	//Set sunlight uniforms
	gl.uniform3f(Map.chunk_shader.sun_dir, sun_dir[0], sun_dir[1], sun_dir[2] );
	gl.uniform3f(Map.chunk_shader.sun_color, sun_color[0], sun_color[1], sun_color[2] );
	gl.uniform1f(Map.chunk_shader.shadow_fudge_factor, Sky.get_shadow_fudge());
	
	//Bind terrain texture
	gl.activeTexture(gl.TEXTURE0);
	gl.bindTexture(gl.TEXTURE_2D, Map.terrain_tex);
	gl.uniform1i(Map.chunk_shader.tex_samp, 0);
	
	//Set shadow map uniforms
	gl.activeTexture(gl.TEXTURE0+1);
	gl.bindTexture(gl.TEXTURE_2D, Shadows.shadow_maps[0].shadow_tex);
	gl.uniform1i(Map.chunk_shader.shadow_tex, 1);
	gl.uniformMatrix4fv(Map.chunk_shader.light_mat, false, Shadows.shadow_maps[0].light_matrix);

	//Set camera
	camera = Game.camera_matrix();
	gl.uniformMatrix4fv(Map.chunk_shader.proj_mat, false, camera);	
	
	//Draw chunks
	for(c in Map.index)
	{
		chunk = Map.index[c];
		if(chunk instanceof Chunk)
			chunk.draw(gl, camera, base_chunk, Map.chunk_shader, false);
	}

	//Draw transparent chunks	
	gl.enable(gl.BLEND);
	gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
	gl.depthMask(0);
	for(c in Map.index)
	{
		chunk = Map.index[c];
		if(chunk instanceof Chunk)
			chunk.draw(gl, camera, base_chunk, Map.chunk_shader, true);
	}
	gl.depthMask(1);

	//Unbind textures
	for(i=0; i<4; ++i)
	{
		gl.activeTexture(gl.TEXTURE0+i);
		gl.bindTexture(gl.TEXTURE_2D, null);
	}	

	//Disable attributes
	gl.disableVertexAttribArray(Map.chunk_shader.pos_attr);
	gl.disableVertexAttribArray(Map.chunk_shader.tc_attr);
	gl.disableVertexAttribArray(Map.chunk_shader.norm_attr);
	gl.disableVertexAttribArray(Map.chunk_shader.light_attr);
	
	//Optional: draw debug information for visibility query
	if(Map.show_debug)
	{
		Debug.draw_tex(Map.vis_tex);
	}	
}

//Draws the map for a shadow shader
Map.draw_shadows = function(gl, shadow_map)
{
	var c, chunk, base_chunk = Player.chunk();
		
	//Draw regular chunks
	for(c in Map.index)
	{
		chunk = Map.index[c];
		if(chunk instanceof Chunk)
			chunk.draw(gl, shadow_map.light_matrix, base_chunk, Shadows.shadow_shader, false);
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
	if(Game.local_ticks % 2 == 1)
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
	Map.num_pending_chunks++;
	
	Map.chunk_worker.postMessage({type: EV_FETCH_CHUNK,
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
	Map.chunk_worker.postMessage({type: EV_SET_BLOCK,
		'x':x, 'y':y, 'z':z, 'b':b});
		
	var bx = (x & CHUNK_X_MASK), 
		by = (y & CHUNK_Y_MASK), 
		bz = (z & CHUNK_Z_MASK);
	return c.set_block(bx, by, bz, b);
}

//Sets the throttle on the vertex buffer generation
Map.set_throttle = function()
{
	Map.chunk_worker.postMessage({type: EV_SET_THROTTLE});
}

//Updates the vertex buffer for a chunk
Map.update_vb = function(x, y, z, verts, ind, tind)
{
	var chunk = Map.lookup_chunk(x, y, z),
		gl = Game.gl;
	
	//Check if chunk vertex buffer is pending
	if(chunk.pending)
	{
		chunk.pending = false;
		Map.num_pending_chunks--;
	}

	chunk.num_elements = ind.length;
	chunk.num_transparent_elements = tind.length;


	if(verts.length > 0)
	{
		if(!chunk.vb)
			chunk.vb	= gl.createBuffer();
		gl.bindBuffer(gl.ARRAY_BUFFER, chunk.vb);	
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(verts), gl.STATIC_DRAW);
	}

	if(ind.length > 0)
	{
		if(!chunk.ib)
			chunk.ib	= gl.createBuffer();

		gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, chunk.ib);
		gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(ind), gl.DYNAMIC_DRAW);
	}
	
	if(tind.length > 0)
	{
		if(!chunk.tib)
			chunk.tib	= gl.createBuffer();
		gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, chunk.tib);
		gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(tind), gl.DYNAMIC_DRAW);
	}
}

//Updates the chunk data
Map.update_chunk = function(x, y, z, data)
{
	var chunk = Map.lookup_chunk(x, y, z);
	chunk.data = data;
}

//Kills off a chunk
Map.forget_chunk = function(str)
{
	var chunk = Map.index[str], gl = Game.gl;
	if(chunk)
	{
		if(chunk.pending)
		{
			Map.num_pending_chunks--;
		}
	
		if(chunk.vb)	gl.deleteBuffer(chunk.vb);
		if(chunk.ib)	gl.deleteBuffer(chunk.ib);
		if(chunk.tib)	gl.deleteBuffer(chunk.tib);
	
		delete Map.index[str];
	}
}

function handle_chunk_event(ev)
{
	switch(ev.data.type)
	{
		case EV_VB_UPDATE:
			Map.vb_workers[Map.worker_idx].postMessage(ev.data);
			Map.worker_idx = (Map.worker_idx+1) % Map.vb_workers.length;
		break;

		case EV_CHUNK_UPDATE:
			Map.update_chunk(
				ev.data.x, ev.data.y, ev.data.z, 
				ev.data.data);
		break;
		
		case EV_PRINT:
			console.log(ev.data.str);				
		break;
		
		case EV_FORGET_CHUNK:
			Map.forget_chunk(ev.data.idx);
		break;
		
		case EV_SET_NON_PENDING:
			var c = Map.lookup_chunk(ev.data.x, ev.data.y, ev.data.z);
			if(c && c.pending)
			{
				c.pending = false;
				Map.num_pending_chunks--;
			}
		break;
	}
}

//This is wasteful, but there doesn't seem to be a better way to get worker-to-worker communication
function handle_vb_complete(ev)
{
	Map.update_vb(
		ev.data.x, ev.data.y, ev.data.z, 
		ev.data.verts, 
		ev.data.ind, 
		ev.data.tind);
		
	Map.chunk_worker.postMessage({type:EV_VB_COMPLETE, x:ev.data.x, y:ev.data.y, z:ev.data.z});
}

//Initialize the web worker
Map.init_worker = function()
{
	//Create vb workers
	Map.vb_workers = new Array(NUM_VB_WORKERS);
	for(var i=0; i<Map.vb_workers.length; ++i)
	{
		Map.vb_workers[i] = new Worker('vb_worker.js');
		Map.vb_workers[i].onmessage = handle_vb_complete;
	}
	Map.worker_idx = 0;

	//Create chunk worker
	Map.chunk_worker = new Worker('chunk_worker.js');
	Map.chunk_worker.onmessage = handle_chunk_event;
	Map.chunk_worker.postMessage({ type: EV_START, key: Session.get_session_id_arr() });
}



Map.shutdown = function()
{
	if(Map.chunk_worker)
		Map.chunk_worker.terminate();
}

Map.get_initial_chunks = function()
{
	//Need to grab all the chunks in the viewable cube around the player
	var c = Player.chunk();
	
	for(var i=c[0] - Math.ceil(Game.zfar / CHUNK_X); i<=c[0] + Math.ceil(Game.zfar / CHUNK_X); i++)
	for(var j=c[1] - Map.chunk_init_radius; j<=c[1] + Map.chunk_init_radius; j++)
	for(var k=c[2] - Math.ceil(Game.zfar / CHUNK_Z); k<=c[2] + Math.ceil(Game.zfar / CHUNK_Z); k++)
	{
		Map.fetch_chunk(i, j, k);
	}
}


