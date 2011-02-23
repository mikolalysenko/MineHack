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
	
	//Start worker
	Map.init_worker();

	return "Ok";
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
	
	if(!chunk)
	{
		chunk = Map.add_chunk(x, y, z);
	}
	
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

//Initialize the web worker
Map.init_worker = function()
{
	var i, vb;
	
	Map.vb_worker = new Worker('chunk_worker.js');

	//Set event handlers
	Map.vb_worker.onmessage = function(ev)
	{
		switch(ev.data.type)
		{
			case EV_VB_UPDATE:
				for(i=0; i<ev.data.vbs.length; ++i)
				{
					vb = ev.data.vbs[i];
					Map.update_vb(vb.x, vb.y, vb.z, vb.d[0], vb.d[1], vb.d[2]);
				}
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
		}
	};

	//Start the worker	
	Map.vb_worker.postMessage({ type: EV_START, key: Session.get_session_id_arr() });
}



Map.shutdown = function()
{
	if(Map.vb_worker)
		Map.vb_worker.terminate();
}

