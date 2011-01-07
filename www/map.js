BlockType =
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
BlockTexCoords =
[
	[ [0,0], [0,0], [0,0] ], //Air
	[ [0,1], [0,1], [0,1] ], //Stone
	[ [0,2], [0,2], [0,2] ], //Dirt
	[ [0,0], [0,3], [0,2] ], //Grass
	[ [1,0], [1,0], [1,0] ], //Cobble
	[ [0,4], [0,4], [0,4] ], //Wood
	[ [1,5], [1,4], [1,5] ]  //Log
];


//The chunk data type
function Chunk(x, y, z, data)
{
	//Set chunk data
	this.data = data;
	
	//Set position
	this.x = x;
	this.y = y;
	this.z = z;
	
	//Leave vertex buffer currently empty for now
	this.vb = null;
	this.ib = null;
	this.num_elements = null;
}

Chunk.prototype.DIMS = [32, 32, 32];

Chunk.prototype.block = function(x, y, z)
{
	if(x < 0 || y < 0 || z < 0 ||
		x >= this.DIMS[0]+2 || 
		y >= this.DIMS[1]+2 || 
		z >= this.DIMS[2]+2)
		return 0;

	return this.data[x + (this.DIMS[0]+2)*(y + (this.DIMS[1]+2)*z) ];
}

Chunk.prototype.calc_ao = function(x, y, z, nx, ny, nz)
{
	if(!Game.calc_ao)
		return 1.0;
		
	//Calculate ambient occlusion (not implemented yet)
	return 1.0;
}

//Construct vertex buffer for this chunk
Chunk.prototype.gen_vb = function(gl)
{
	var vertices = new Array();
	var indices  = new Array();
	var tex_coords = new Array();
	var n_elements = 0;
	var nv = 0;
	
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
		dt = 1.0 / 16.0;
		

		tex_coords.push(tx);
		tex_coords.push(ty+dt);

		tex_coords.push(tx);
		tex_coords.push(ty);

		tex_coords.push(tx+dt);
		tex_coords.push(ty);


		tex_coords.push(tx+dt);
		tex_coords.push(ty+dt);
		
	}
	
	for(var x=1; x<=this.DIMS[0]; ++x)
	for(var y=1; y<=this.DIMS[1]; ++y)
	for(var z=1; z<=this.DIMS[2]; ++z)
	{
		var block_id = this.block(x,y,z);
		
		if(block_id == 0)
			continue;
		
		if(this.block(x-1,y,z) == 0)
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
		
		if(this.block(x+1,y,z) == 0)
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
		
		if(this.block(x,y-1,z) == 0)
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
		
		if(this.block(x,y+1,z) == 0)
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
		
		if(this.block(x,y,z-1) == 0)
		{
			//Add -z face
			add_face();
			
			appendv([
				[x,  y+1,z],
				[x,  y,  z],
				[x+1,y,  z],
				[x+1,y+1,z]]);
				
			add_tex_coord(block_id, 1);
		}
		
		if(this.block(x,y,z+1) == 0)
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
	
	this.vb = gl.createBuffer();
	gl.bindBuffer(gl.ARRAY_BUFFER, this.vb);	
	gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertices), gl.STATIC_DRAW);
	
	this.ib = gl.createBuffer();
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.ib);
	gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(indices), gl.STATIC_DRAW);
	
	this.tb = gl.createBuffer();
	gl.bindBuffer(gl.ARRAY_BUFFER, this.tb);
	gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(tex_coords), gl.STATIC_DRAW);
}

Chunk.prototype.draw = function(gl, chunk_shader)
{
	if(this.vb == null)
		this.gen_vb(gl);

	var pos = new Float32Array([1, 0, 0, 0,
								0, 1, 0, 0,
								0, 0, 1, 0,
								this.x*this.DIMS[0], this.y*this.DIMS[1], this.z*this.DIMS[2], 1]);
	
	gl.uniformMatrix4fv(chunk_shader.view_mat, false, pos);
	gl.bindBuffer(gl.ARRAY_BUFFER, this.vb);
	gl.vertexAttribPointer(chunk_shader.pos_attr, 3, gl.FLOAT, false, 0, 0);
	
	gl.bindBuffer(gl.ARRAY_BUFFER, this.tb);
	gl.vertexAttribPointer(chunk_shader.tc_attr, 2, gl.FLOAT, false, 0, 0);

	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.ib);
	gl.drawElements(gl.TRIANGLES, this.num_elements, gl.UNSIGNED_SHORT, 0);
}

Chunk.prototype.release = function(gl)
{
	//Release chunk here
}

var Map =
{
	chunks : [],
	terrain_tex : null
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
	
	for(var i=0; i<Map.chunks.length; i++)
	{
		var c = Map.chunks[i];
		c.draw(gl, Map.chunk_shader);
	}
}

//Adds a chunk to the list
Map.add_chunk = function(chunk)
{
	Map.chunks.push(chunk);
}

//Returns the block type for the give position
Map.get_block = function(pos)
{
	return 0;
}
