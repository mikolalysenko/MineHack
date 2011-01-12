const BlockType =
[
	"Air",
	"Stone",
	"Dirt",
	"Grass",
	"Cobblestone",
	"Wood",
	"Log"
];

//Storage format:
// 0 = top
// 1 = side
// 2 = bottom
//Like minecraft, blocks can have 3 different special labels
const BlockTexCoords =
[
	[ [0,0], [0,0], [0,0] ], //Air
	[ [0,1], [0,1], [0,1] ], //Stone
	[ [0,2], [0,2], [0,2] ], //Dirt
	[ [0,0], [0,3], [0,2] ], //Grass
	[ [1,0], [1,0], [1,0] ], //Cobble
	[ [0,4], [0,4], [0,4] ], //Wood
	[ [1,5], [1,4], [1,5] ]  //Log
];

function ChunkVB(p, 
	x_min, y_min, z_min,
	x_max, y_max, z_max)
{
	this.vb = null;
	this.ib = null;
	this.tb = null;
	this.num_elements = 0;
	this.p 		 = p;
	this.x_min   = x_min;
	this.y_min   = y_min;
	this.z_min   = z_min;
	this.x_max	 = x_max;
	this.y_max	 = y_max;
	this.z_max	 = z_max;
	this.dirty   = true;
}

//Sets the vb to dirty, will regenerate as needed
ChunkVB.prototype.set_dirty = function(gl)
{
	this.dirty = true;
}

//Construct vertex buffer for this chunk
ChunkVB.prototype.gen_vb = function(gl)
{
	var vertices = new Array();
	var indices  = new Array();
	var tex_coords = new Array();
	var n_elements = 0;
	var nv = 0;
	var p = this.p;
	
	var appendv = function(v)
	{
		for(var i=0; i<v.length; i++)
		{
			vertices.push(v[i][0] - 0.5);
			vertices.push(v[i][1] - 0.5);
			vertices.push(v[i][2] - 0.5);
			
			++nv;
		}
	}
	
	var add_face = function()
	{
		indices.push(nv);
		indices.push(nv+1);
		indices.push(nv+2);
		indices.push(nv);
		indices.push(nv+2);
		indices.push(nv+3);
	}
	
	var add_tex_coord = function(block_t, dir)
	{
		tc = BlockTexCoords[block_t][dir];
		
		tx = tc[1] / 16.0;
		ty = tc[0] / 16.0;
		dt = 1.0 / 16.0 - 1.0/256.0;
		
		tex_coords.push(tx);
		tex_coords.push(ty+dt);

		tex_coords.push(tx);
		tex_coords.push(ty);

		tex_coords.push(tx+dt);
		tex_coords.push(ty);

		tex_coords.push(tx+dt);
		tex_coords.push(ty+dt);	
	}
	
	var data = p.data;
	
	var left	= Map.lookup_chunk(p.x-1, p.y, p.z),
		right	= Map.lookup_chunk(p.x+1, p.y, p.z),
		bottom	= Map.lookup_chunk(p.x, p.y-1, p.z),
		top 	= Map.lookup_chunk(p.x, p.y+1, p.z),
		front	= Map.lookup_chunk(p.x, p.y, p.z-1),
		back	= Map.lookup_chunk(p.x, p.y, p.z+1);
		
	var d_lx = null, d_ux = null,
		d_ly = null, d_uy = null,
		d_lz = null, d_uz = null;
		
	if(left)	d_lx = left.data;
	if(right)	d_ux = right.data;
	if(bottom)	d_ly = bottom.data;
	if(top)		d_uy = top.data;
	if(front)	d_lz = front.data;
	if(back)	d_uz = back.data;
	
	for(var x=this.x_min; x<this.x_max; ++x)
	for(var y=this.y_min; y<this.y_max; ++y)
	for(var z=this.z_min; z<this.z_max; ++z)
	{
		var idx = x + (y<<CHUNK_X_S) + (z<<(CHUNK_XY_S));
		var block_id = data[idx];
		
		if(block_id == 0)
			continue;
		
		if(	(x == 0 && 
				d_lx != null && 
				d_lx[CHUNK_X_MASK 				 + 
					(y				<<CHUNK_X_S) + 
					(z				<<CHUNK_XY_S)] == 0 ) ||
			(x > 0 && 
				data[idx-CHUNK_X_STEP] == 0) )
		{
			//Add -x face
			add_face();
			
			appendv( [
				[x,y  ,z  ],
				[x,y+1,z  ],
				[x,y+1,z+1],
				[x,y  ,z+1] ]);
				
			add_tex_coord(block_id, 1);
		}
		
		if(	(x == CHUNK_X-1 && 
				d_ux != null && 
				d_ux[(y				<<CHUNK_X_S) + 
					 (z				<<CHUNK_XY_S)] == 0) ||
			(x < CHUNK_X-1 && 
				data[idx+CHUNK_X_STEP] == 0) )
		{
			//Add +x face
			add_face();
			
			appendv([
				[x+1,y,  z  ],
				[x+1,y+1,z  ],
				[x+1,y+1,z+1],
				[x+1,y,  z+1]
				]);
				
			add_tex_coord(block_id, 1);
		}
		
		if(	(y == 0 && 
				d_ly != null && 
				d_ly[x 						    + 
					(CHUNK_Y_MASK << CHUNK_X_S) + 
					(z			  << CHUNK_XY_S)] == 0) ||
			(y > 0 && 
				data[idx-CHUNK_Y_STEP] == 0) )
		{
			//Add -y face
			add_face();
			
			appendv([
				[x,  y,  z  ],
				[x,  y,  z+1],
				[x+1,y,  z+1],
				[x+1,y,  z  ]]);
				
			add_tex_coord(block_id, 2);
		}
		
		if(	(y == CHUNK_Y-1 && 
				d_uy != null && 
				d_uy[x + (z << CHUNK_XY_S)] == 0) ||
			(y < CHUNK_Y-1 && 
				data[idx+CHUNK_Y_STEP] == 0) )
		{
			//Add +y face
			add_face();
			
			appendv([
				[x,  y+1,  z  ],
				[x+1,y+1,  z  ],
				[x+1,y+1,  z+1],
				[x,  y+1,  z+1]]);
				
			add_tex_coord(block_id, 0);
		}
		
		if(	(z == 0 && 
				d_lz != null && 
				d_lz[x + 
					(y				<<CHUNK_X_S) + 
					(CHUNK_Z_MASK	<<CHUNK_XY_S)] == 0) ||
			(z > 0 && 
				data[idx-CHUNK_Z_STEP] == 0) )
		{
			//Add -z face
			add_face();
			
			appendv([
				[x,  y,  z],
				[x,  y+1,z],
				[x+1,y+1,z],
				[x+1,y,  z]
			]);
				
			add_tex_coord(block_id, 1);
		}
		
		if(	(z == CHUNK_Z-1 && 
				d_uz != null && 
				d_uz[x + (y<<CHUNK_X_S)] == 0) ||
			(z < CHUNK_Z-1 && 
				data[idx+CHUNK_Z_STEP] == 0) )
		{
			//Add +z face
			add_face();
			
			appendv([
				[x,  y,  z+1],
				[x,  y+1,z+1],
				[x+1,y+1,z+1],
				[x+1,y,  z+1]]);
				
			add_tex_coord(block_id, 1);
		}
	}

	this.num_elements = indices.length;
	
	if(this.vb == null)
		this.vb = gl.createBuffer();
	gl.bindBuffer(gl.ARRAY_BUFFER, this.vb);	
	gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertices), gl.DYNAMIC_DRAW);
	
	if(this.ib == null)
		this.ib = gl.createBuffer();
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.ib);
	gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(indices), gl.DYNAMIC_DRAW);
	
	if(this.tb == null)
		this.tb = gl.createBuffer();
	gl.bindBuffer(gl.ARRAY_BUFFER, this.tb);
	gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(tex_coords), gl.DYNAMIC_DRAW);
	
	//Clean up temporary data
	delete vertices;
	delete indices;
	delete tex_coords;
	
	//No longer need to generate
	this.dirty = false;
}

//Draws a chunk
ChunkVB.prototype.draw = function(gl, chunk_shader)
{
	if(this.dirty)
		this.gen_vb(gl);

	gl.bindBuffer(gl.ARRAY_BUFFER, this.vb);
	gl.vertexAttribPointer(chunk_shader.pos_attr, 3, gl.FLOAT, false, 0, 0);
	
	gl.bindBuffer(gl.ARRAY_BUFFER, this.tb);
	gl.vertexAttribPointer(chunk_shader.tc_attr, 2, gl.FLOAT, false, 0, 0);

	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.ib);
	gl.drawElements(gl.TRIANGLES, this.num_elements, gl.UNSIGNED_SHORT, 0);
}

//Releases the resources associated to a chunk
ChunkVB.prototype.release = function(gl)
{
	if(this.vb)
		gl.deleteBuffer(this.vb);
	if(this.ib)
		gl.deleteBuffer(this.ib);
	if(this.tb)
		gl.deleteBuffer(this.tb);
		
	delete this.vb;
	delete this.ib;
	delete this.tb;
}

//The chunk data type
function Chunk(x, y, z, data)
{
	//Set chunk data
	this.data = data;
	
	//Update last tick
	this.last_tick = Game.tick_count;
	
	//Set position
	this.x = x;
	this.y = y;
	this.z = z;
	
	//Create vertex buffers for facets
	this.int_vb = new ChunkVB(this, 
		1, 1, 1,
		this.DIMS[0]-1, this.DIMS[1]-1, this.DIMS[2]-1);
		
	this.left_vb = new ChunkVB(this, 
		0, 1, 1,
		1, this.DIMS[1]-1, this.DIMS[2]-1);
		
	this.right_vb = new ChunkVB(this, 
		this.DIMS[0]-1, 1, 1,
		this.DIMS[0], this.DIMS[1]-1, this.DIMS[2]-1);

	this.bottom_vb = new ChunkVB(this, 
		1, 0, 1,
		this.DIMS[0]-1, 1, this.DIMS[2]-1);
		
	this.top_vb = new ChunkVB(this, 
		1, this.DIMS[1]-1, 1,
		this.DIMS[0]-1, this.DIMS[1], this.DIMS[2]-1);
		
	this.front_vb = new ChunkVB(this, 
		1, 1, 0,
		this.DIMS[0]-1, this.DIMS[1]-1, 1);
		
	this.back_vb = new ChunkVB(this, 
		1, 1, this.DIMS[2]-1,
		this.DIMS[0]-1, this.DIMS[1]-1, this.DIMS[2]);
	
	this.corner_vbs = [
	
		new ChunkVB(this,
			0,            this.DIMS[1]-1, this.DIMS[2]-1,
			this.DIMS[0], this.DIMS[1],   this.DIMS[2]),

		new ChunkVB(this,
			0,            this.DIMS[1]-1, 0,
			this.DIMS[0], this.DIMS[1],   1),

		new ChunkVB(this,
			0,              this.DIMS[1]-1, 1,
			1,              this.DIMS[1],   this.DIMS[2]-1),
	
		new ChunkVB(this,
			this.DIMS[0]-1, this.DIMS[1]-1, 1,
			this.DIMS[0],   this.DIMS[1],   this.DIMS[2]-1),

		new ChunkVB(this,
			0,              0,             0,
			this.DIMS[0],   1,             1),

		new ChunkVB(this,
			0,              0,             this.DIMS[2]-1,
			this.DIMS[0],   1,             this.DIMS[2]),			

		new ChunkVB(this,
			0,			    0,			  1,
			1,			    1,			  this.DIMS[2]-1),
			
		new ChunkVB(this,
			this.DIMS[0]-1, 0,			  1,
			this.DIMS[0],   1,			  this.DIMS[2]-1),

		new ChunkVB(this,
			this.DIMS[0]-1, 1,                0,
			this.DIMS[0],   this.DIMS[1] - 1, 1),

		new ChunkVB(this,
			this.DIMS[0]-1, 1,                this.DIMS[2]-1,
			this.DIMS[0],   this.DIMS[1] - 1, this.DIMS[2]),
		
		new ChunkVB(this,
			0, 				1,                0,
			1, 				this.DIMS[1] - 1, 1),
		
		new ChunkVB(this,
			0,				1,                this.DIMS[2]-1,
			1, 				this.DIMS[1] - 1, this.DIMS[2])
	];
}

//The dimensions of a chunk
Chunk.prototype.DIMS = [32, 32, 32];

//Returns true of the chunk is in the frustum
Chunk.prototype.in_frustum = function(m)
{
	var c = Player.chunk();
	var v = [(this.x-c[0])*32,    (this.y-c[1])*32,    (this.z-c[2])*32];
	var in_p = 0;
	
	
	for(var dx=-1; dx<=32; dx+=33)
	for(var dy=-1; dy<=32; dy+=33)
	for(var dz=-1; dz<=32; dz+=33)	
	{
		var q = [v[0] + dx, v[1] + dy, v[2] + dz];
		
		var w = q[0]*m[3] + q[1]*m[7] + q[2]*m[11] + m[15];
		var x = q[0]*m[0] + q[1]*m[4] + q[2]*m[8]  + m[12];
		
		if(x <=  w) in_p |= 1;
		if(x >= -w) in_p |= 2;
		if(in_p == 63)
			return true;
		
		var y = q[0]*m[1] + q[1]*m[5] + q[2]*m[9]  + m[13];
		if(y <=  w) in_p |= 4;
		if(y >= -w) in_p |= 8;
		if(in_p == 63)
			return true;
			
		var z = q[0]*m[2] + q[1]*m[6] + q[2]*m[10] + m[14];
		if(z <=  w) in_p |= 16;
		if(z >= -w) in_p |= 32;
		if(in_p == 63)
			return true;
	}

	return false;
}

//Sets the block type and updates vertex buffers as needed
Chunk.prototype.set_block = function(x, y, z, b)
{
	this.data[x + this.DIMS[0]*(y + this.DIMS[1]*z) ] = b;
	this.int_vb.set_dirty();

	var corner = (x < 1 || x >= this.DIMS[0] - 1 ||
				  y < 1 || y >= this.DIMS[1] - 1 ||
				  z < 1 || z >= this.DIMS[2] - 1);
	if(corner)
	{
		this.dirty_corners(Game.gl);
	}

	if(x < 2)
	{
		this.left_vb.set_dirty();
		if(x < 1)
		{
			var c = Map.lookup_chunk(this.x-1, this.y, this.z);
			if(c)
			{
				c.right_vb.set_dirty();
				if(corner)
					c.dirty_corners();
			}
		}
	}
	if(x >= this.DIMS[0] - 2)
	{
		this.right_vb.set_dirty();
		if(x >= this.DIMS[0] - 1)
		{
			var c = Map.lookup_chunk(this.x+1, this.y, this.z);
			if(c)
			{
				c.left_vb.set_dirty();
				if(corner)
					c.dirty_corners();
			}
		}
	}


	if(y < 2)
	{
		this.bottom_vb.set_dirty();
		if(y < 1)
		{
			var c = Map.lookup_chunk(this.x, this.y-1, this.z);
			c.top_vb.set_dirty();
			if(corner)
				c.dirty_corners();
		}
	}
	if(y >= this.DIMS[1] - 2)
	{
		this.top_vb.set_dirty();
		if(y >= this.DIMS[1] - 1)
		{
			var c = Map.lookup_chunk(this.x, this.y+1, this.z);
			if(c)
			{
				c.bottom_vb.set_dirty();
				if(corner)
					c.dirty_corners();
			}
		}
	}

	if(z < 2)
	{
		this.front_vb.set_dirty();
		if(z < 1)
		{
			var c = Map.lookup_chunk(this.x, this.y, this.z-1);
			c.back_vb.set_dirty();
			if(corner)
				c.dirty_corners();
		}
	}
	if(z >= this.DIMS[2] - 2)
	{
		this.back_vb.set_dirty();
		if(z >= this.DIMS[2] - 1)
		{
			var c = Map.lookup_chunk(this.x, this.y, this.z+1);
			if(c)
			{
				c.front_vb.set_dirty();
				if(corner)
					c.dirty_corners();
			}
		}
	}
}

//Sets the dirty flag on the corners
Chunk.prototype.dirty_corners = function()
{
	for(var i=0; i<12; i++)
		this.corner_vbs[i].set_dirty();
}

Chunk.prototype.dirty_vbs = function()
{
	this.int_vb.set_dirty();
	this.top_vb.set_dirty();
	this.bottom_vb.set_dirty();
	this.left_vb.set_dirty();
	this.right_vb.set_dirty();
	this.front_vb.set_dirty();
	this.back_vb.set_dirty();
	this.dirty_corners();
}

//Forces a chunk to regenerate its vbs
Chunk.prototype.force_regen = function(gl)
{
	this.int_vb.gen_vb(gl);
	this.left_vb.gen_vb(gl);
	this.right_vb.gen_vb(gl);
	this.top_vb.gen_vb(gl);
	this.bottom_vb.gen_vb(gl);
	this.back_vb.gen_vb(gl);
	this.front_vb.gen_vb(gl);
	for(var i=0; i<12; i++)
		this.corner_vbs[i].gen_vb(gl);
}

//Draws the chunk
Chunk.prototype.draw = function(gl, chunk_shader, cam)
{
	if(!this.in_frustum(cam))
		return;
		
	this.last_tick = Game.tick_count;

	var c = Player.chunk();

	var pos = new Float32Array([1, 0, 0, 0,
								0, 1, 0, 0,
								0, 0, 1, 0,
								(this.x-c[0])*this.DIMS[0], 
								(this.y-c[1])*this.DIMS[1], 
								(this.z-c[2])*this.DIMS[2], 1]);
	
	gl.uniformMatrix4fv(chunk_shader.view_mat, false, pos);
	
	this.int_vb.draw(gl, chunk_shader);
	this.top_vb.draw(gl, chunk_shader);
	this.bottom_vb.draw(gl, chunk_shader);
	this.left_vb.draw(gl, chunk_shader);
	this.right_vb.draw(gl, chunk_shader);
	this.front_vb.draw(gl, chunk_shader);
	this.back_vb.draw(gl, chunk_shader);
	
	for(var i=0; i<12; ++i)
		this.corner_vbs[i].draw(gl, chunk_shader);
}

//Releases a chunk and its associated resources
Chunk.prototype.release = function(gl)
{
	this.int_vb.release(gl);
	delete this.int_vb;
	this.left_vb.release(gl);
	delete this.left_vb;
	this.right_vb.release(gl);
	delete this.right_vb;
	this.bottom_vb.release(gl);
	delete this.bottom_vb;
	this.top_vb.release(gl);
	delete this.top_vb;
	this.front_vb.release(gl);
	delete this.front_vb;
	this.back_vb.release(gl);
	delete this.back_vb;
	
	for(var i=0; i<12; i++)
	{
		this.corner_vbs[i].release(gl);
		delete this.corner_vbs[i];
	}
	
	delete this.data;
}

//The map
var Map =
{
	index			: {},	//The chunk index
	terrain_tex		: null,
	max_chunks		: 1024,
	chunk_count 	: 0,
	pending_chunks	: 0
};

Map.init = function(gl)
{
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
	
	return "Ok";
}

//Updates the cache of chunks
Map.update_cache = function()
{
	//Need to grab all the chunks in the 5x5x5 cube around the player
	var c = Player.chunk();
	
	for(var i=c[0] - 2; i<=c[0] + 2; i++)
	for(var j=c[1] - 2; j<=c[1] + 2; j++)
	for(var k=c[2] - 2; k<=c[2] + 2; k++)
	{
		Map.fetch_chunk(i, j, k);
	}
	
	//If there are no pending chunks, grab a few random chunks from the boundary to help speed up loading
	if(Map.pending_chunks == 0)
	{
		//TODO: Precache chunks near player
	}
	
	//If we are over the chunk count, remove old chunks
	if(Map.chunk_count > Map.max_chunks)
	{
		//TODO: Purge old chunks
	}
}

//Draws the map
// Input:
//  gl - the open gl rendering context
//  camera - the current camera matrix
Map.draw = function(gl, camera)
{
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
		Map.index[c].draw(gl, Map.chunk_shader, camera);
	}
}

Map.decompress_chunk = function(arr, data)
{
	if(arr.length == 0)
		return null;

	var i = 0;
	for(var k=0; k<arr.length; )
	{
		var n = arr[k];
		++k;

		if(n == 0xff)
		{
			var n = arr[k] + (arr[k+1] << 8);
			k += 2;
		}
		else
		{
			n++;
		}
		
		var c = arr[k];
		++k;
		
		for(var j=0; j<n; ++j)
		{
			data[i++] = c;
		}
	}
	
	return data;
}

//Downloads a chunk from the server
Map.fetch_chunk = function(x, y, z)
{
	//If chunk is already stored, don't get it
	if(Map.lookup_chunk(x,y,z))
		return;

	++Map.pending_chunks;

	//Add new chunk, though leave it empty
	var chunk = new Chunk(x, y, z, new Uint8Array(32*32*32));
	Map.add_chunk(chunk);

	asyncGetBinary("g?k="+Session.session_id+"&x="+x+"&y="+y+"&z="+z, function(arr)
	{	
		if(arr.length == 0)
			Map.remove_chunk(chunk);
		
		//Make sure chunk hasn't been unloaded
		if(Map.lookup_chunk(x,y,z))
		{
			Map.decompress_chunk(arr, chunk.data);
			chunk.dirty_vbs()
			
			//Regenerate vertex buffers for neighboring chunks
			var c = Map.lookup_chunk(chunk.x-1, chunk.y, chunk.z);
			if(c)
			{
				c.right_vb.set_dirty();
				c.dirty_corners();
			}
			c = Map.lookup_chunk(chunk.x+1, chunk.y, chunk.z);
			if(c)
			{
				c.left_vb.set_dirty();
				c.dirty_corners();
			}
			c = Map.lookup_chunk(chunk.x, chunk.y-1, chunk.z);
			if(c)
			{
				c.top_vb.set_dirty();
				c.dirty_corners();
			}
			c = Map.lookup_chunk(chunk.x, chunk.y+1, chunk.z);
			if(c)
			{
				c.bottom_vb.set_dirty();
				c.dirty_corners();
			}
			c = Map.lookup_chunk(chunk.x, chunk.y, chunk.z-1);
			if(c)
			{
				c.back_vb.set_dirty();
				c.dirty_corners();
			}
			c = Map.lookup_chunk(chunk.x, chunk.y, chunk.z+1);
			if(c)
			{
				c.front_vb.set_dirty();
				c.dirty_corners();
			}
			
			chunk.force_regen(Game.gl);
		}
		
		--Map.pending_chunks;
	});
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
	if(Map.lookup_chunk)
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
	var cx = (x >> 5), cy = (y >> 5), cz = (z >> 5);
	var c = Map.lookup_chunk(cx, cy, cz);		
	if(!c)
		return -1;
	
	var bx = (x & 31), by = (y & 31), bz = (z & 31);
	return c.data[bx + ((by + (bz<<5))<<5)];
}

//Sets a block in the map to the given type
Map.set_block = function(x, y, z, b)
{
	var cx = (x >> 5), cy = (y >> 5), cz = (z >> 5);
	var c = Map.lookup_chunk(cx, cy, cz);		
	if(!c)
		return;
	
	var bx = (x & 31), by = (y & 31), bz = (z & 31);
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
	
	//Step block-by-bloc along ray
	var t = 0.0;
	
	var norm = [0, 0, 0];
	
	while(t <= max_d)
	{
		var b = Map.get_block(Math.round(ox), Math.round(oy), Math.round(oz));
		if(b != 0)
			return [ox, oy, oz, b, norm[0], norm[1], norm[2]];
			
		var step = 1.0;
		
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

