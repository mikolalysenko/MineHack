"use strict";

const BlockType =
[
	"Air",
	"Stone",
	"Dirt",
	"Grass",
	"Cobblestone",
	"Wood",
	"Log",
	"Water",
	"Sand"
];

//Storage format:
// 0 = top
// 1 = side
// 2 = bottom
//Indices into tile map are of the form (row x column)
const BlockTexCoords =
[
	[ [0,0], [0,0], [0,0] ], //Air
	[ [0,1], [0,1], [0,1] ], //Stone
	[ [0,2], [0,2], [0,2] ], //Dirt
	[ [0,0], [0,3], [0,2] ], //Grass
	[ [1,0], [1,0], [1,0] ], //Cobble
	[ [0,4], [0,4], [0,4] ], //Wood
	[ [1,5], [1,4], [1,5] ],  //Log
	[ [12,15], [12,15], [12,15] ],  //Water	
	[ [1,2], [1,2], [1,2] ]  //Sand	
];

//If true, then block type is transparent
const Transparent =
[
	true,	//Air
	false,	//Stone
	false, 	//Dirt
	false,	//Grass
	false, 	//Cobble
	false,	//Wood
	false,	//Log
	true, 	//Water
	false,	//Sand
];


//Format is:
// Transmission, Scatter, Reflectance, Emissivity
const BlockMaterials =
[
	[ 1.0, 0.01, 0, 0 ],	//Air
	[ 0, 0, 0.7, 0 ],		//Stone
	[ 0, 0, 0.5, 0 ],		//Dirt
	[ 0, 0, 1, 0 ],		//Grass
	[ 0, 0, 0.55, 0 ],		//Cobble
	[ 0, 0, 0.8, 0 ],		//Wood
	[ 0, 0, 0.6, 0 ],		//Log
	[ 0.8, 0.15, 0, 0 ],	//Water
	[ 0, 0, 0.95, 0 ]		//Sand
];

//Block face index
const LEFT		= 0;
const RIGHT		= 1;
const BOTTOM	= 2;
const TOP		= 3;
const FRONT		= 4;
const BACK		= 5;

function ChunkVB(p, 
	x_min, y_min, z_min,
	x_max, y_max, z_max)
{
	this.empty = false;
	this.tempty = false;
	this.vb = null;
	this.ib = null;
	this.tib = null;
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
	this.dirty	= true;
	this.empty	= false;
	this.tempty = false;
}

//Allocate an empty buffer for unloaded chunks
ChunkVB.prototype.empty_data = new Uint8Array(CHUNK_SIZE);

//Construct vertex buffer for this chunk
// This code makes me want to barf - Mik
ChunkVB.prototype.gen_vb = function(gl)
{
	var vertices = [],
		indices  = [],
		tindices = [],
		nv = 0, x, y, z, p = this.p,
	
	//var neighborhood = new Uint32Array(27); (too slow goddammit.  variant arrays even worse)
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
	
	//Turn off dirty flag
	this.dirty = false;
		
		
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
	
	
	this.empty	= indices.length == 0;
	this.tempty	= tindices.length == 0;

	if(this.empty && this.tempty)
	{
	/*
		//Clean up temporary data
		delete vertices;
		delete indices;
		delete tindices;
		delete tex_coords;
	*/
		return;
	}

	this.num_elements = indices.length;
	this.num_transparent_elements = tindices.length;
	
	if(this.vb == null)
		this.vb = gl.createBuffer();
	gl.bindBuffer(gl.ARRAY_BUFFER, this.vb);	
	gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertices), gl.DYNAMIC_DRAW);
	
	if(this.ib == null)
		this.ib = gl.createBuffer();
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.ib);
	gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(indices), gl.DYNAMIC_DRAW);
	
	if(this.tib == null)
		this.tib = gl.createBuffer();
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.tib);
	gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(tindices), gl.DYNAMIC_DRAW);
	
	/*
	//Clean up temporary data
	delete vertices;
	delete indices;
	delete tindices;
	*/
}

//Draws a chunk
ChunkVB.prototype.draw = function(gl, chunk_shader, transp)
{
	if(this.pending)
		return;

	if(this.dirty)
		this.gen_vb(gl);

	gl.bindBuffer(gl.ARRAY_BUFFER, this.vb);
	gl.vertexAttribPointer(chunk_shader.pos_attr,	4, gl.FLOAT, false, 32, 0);
	gl.vertexAttribPointer(chunk_shader.tc_attr, 	4, gl.FLOAT, false, 32, 16);

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

ChunkVB.prototype.draw_vis = function(gl, vis_shader)
{
	if(this.dirty)
		this.gen_vb(gl);

	gl.bindBuffer(gl.ARRAY_BUFFER, this.vb);
	gl.vertexAttribPointer(vis_shader.pos_attr,	4, gl.FLOAT, false, 32, 0);
	
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
	if(this.tib)
		gl.deleteBuffer(this.tib);
	if(this.tb)
		gl.deleteBuffer(this.tb);
		
	delete this.vb;
	delete this.ib;
	delete this.tib;
	delete this.tb;
}

//The chunk data type
function Chunk(x, y, z, data)
{
	//Set chunk data
	this.data = data;
	
	//Set pending
	this.pending = true;
	
	this.is_air = false;
	
	//Set position
	this.x = x;
	this.y = y;
	this.z = z;
	
	//Pending blocks to write
	this.pending_blocks = [];
	
	//Create vertex buffers for facets
	this.vb = new ChunkVB(this, 
		0, 0, 0,
		CHUNK_X, CHUNK_Y, CHUNK_Z);
}

//Returns true of the chunk is in the frustum
Chunk.prototype.in_frustum = function(m)
{
	var c = Player.chunk();
	var vx = (this.x-c[0])*CHUNK_X,
		vy = (this.y-c[1])*CHUNK_Y,
		vz = (this.z-c[2])*CHUNK_Z,
		qx, qy, qz, in_p = 0, w, x, y, z;
	
	
	for(var dx=-1; dx<=CHUNK_X; dx+=CHUNK_X+1)
	for(var dy=-1; dy<=CHUNK_Y; dy+=CHUNK_Y+1)
	for(var dz=-1; dz<=CHUNK_Z; dz+=CHUNK_Z+1)	
	{
		qx = dx + vx;
		qy = dy + vy;
		qz = dz + vz;
	
		w = qx*m[3] + qy*m[7] + qz*m[11] + m[15];
		x = qx*m[0] + qy*m[4] + qz*m[8]  + m[12];
		
		if(x <=  w) in_p |= 1;
		if(in_p == 63)	return true;
		if(x >= -w) in_p |= 2;
		if(in_p == 63)	return true;
		
		y = qx*m[1] + qy*m[5] + qz*m[9]  + m[13];
		if(y <=  w) in_p |= 4;
		if(in_p == 63)	return true;
		if(y >= -w) in_p |= 8;
		if(in_p == 63)	return true;
			
		z = qx*m[2] + qy*m[6] + qz*m[10] + m[14];
		if(z <=  w) in_p |= 16;
		if(in_p == 63)	return true;
		if(z >= -w) in_p |= 32;
		if(in_p == 63)	return true;
	}

	return false;
}

//Brute force calculate the height
Chunk.prototype.brute_force_height = function(x, z, cell)
{
	for(var y = CHUNK_Y-1; y>=0; --y)
	{
		if(!Transparent[this.data[x + (y<<CHUNK_X_S) + (z<<CHUNK_XY_S)]])
		{
			cell[x + (z<<CHUNK_X_S)] = y + (this.y<<CHUNK_Y_S);
			return true;
		}
	}
	
	return false;
}

//Sets the block type and updates vertex buffers as needed
Chunk.prototype.set_block = function(x, y, z, b)
{
	if(this.pending)
	{
		this.pending_blocks.push( [x, y, z, b] );
		return;
	}

	var pb = this.data[x + (y<<CHUNK_X_S) + (z<<CHUNK_XY_S)];
	
	if(pb == b)
		return;
		
	this.is_air = false;

	this.data[x + (y<<CHUNK_X_S) + (z<<CHUNK_XY_S)] = b;
	this.vb.set_dirty();

	var coord = [x,y,z];
	var delta = [[1,0,0],
				 [0,1,0],
				 [0,0,1]];
				 
	for(var i=0; i<3; i++)
	{
		var d = delta[i];
		if(coord[i] == 0)
		{
			var c = Map.lookup_chunk(
				this.x - d[0],
				this.y - d[1],
				this.z - d[2]);
			if(c)
				c.vb.set_dirty();
		}
		else if(coord[i] == CHUNK_DIMS[i] - 1)
		{
			var c = Map.lookup_chunk(
				this.x + d[0],
				this.y + d[1],
				this.z + d[2]);
			if(c)
				c.vb.set_dirty();
		}
	}
	
	//Update surface
	if(Transparent[pb] == Transparent[b])
		return;
		
	var cell = Map.surface_index[this.x + ":" + this.z];
	var pheight = cell[x + (z<<CHUNK_X_S)],
		nheight = y + (this.y << CHUNK_Y_S);
		
		
	if(pheight > nheight)
		return;
	
	if(Transparent[b])
	{
		//Set this flag to force us to drill down
		cell[x + (z << CHUNK_X_S)] = 0;
	
		//Need to drill down to find new surface value
		for(var j=this.y; j>0; --j)
		{
			var c = Map.lookup_chunk(this.x, j, this.z);
			if(c)
			{
				//Set dirty flags for light changes
				for(var dx=this.x-1; dx<this.x+1; ++dx)
				for(var dz=this.z-1; dz<this.z+1; ++dz)
				{
					var q = Map.lookup_chunk(dx, j, dz);
					if(q)
						q.vb.set_dirty();
				}
				
				if(c.brute_force_height(x, z, cell))
					break;
			}
			else
			{
				Map.fetch_chunk(this.x, j, this.z);
				break;
			}
		}
	}
	else
	{
		if(nheight == pheight)
			return;
	
		cell[x + (z<<CHUNK_X_S)] = nheight;
	
		//Set dirty flags for light changes
		for(var j=this.y; j>=(pheight>>CHUNK_Y_S); --j)
		for(var dx=this.x-1; dx<this.x+1; ++dx)
		for(var dz=this.z-1; dz<this.z+1; ++dz)
		{
			var q = Map.lookup_chunk(dx, j, dy);
			if(q)
				q.vb.set_dirty();
		}
	}

}

//Forces a chunk to regenerate its vbs
Chunk.prototype.force_regen = function(gl)
{
	this.vb.gen_vb(gl);
}

//Draws the chunk
Chunk.prototype.draw = function(gl, chunk_shader, cam, transp)
{
	if(this.pending || this.is_air)
		return;
	if(!transp)
	{
		if(this.vb.empty)
			return;
	}
	else if(this.vb.tempty)
	{
		return;
	}
	else if(!this.in_frustum(cam))
	{
		return;
	}
		
	var c = Player.chunk();
	var pos = new Float32Array([1, 0, 0, 0,
								0, 1, 0, 0,
								0, 0, 1, 0,
								(this.x-c[0])*CHUNK_X, 
								(this.y-c[1])*CHUNK_Y, 
								(this.z-c[2])*CHUNK_Z, 1]);
	
	gl.uniformMatrix4fv(chunk_shader.view_mat, false, pos);
	
	this.vb.draw(gl, chunk_shader, transp);
}

Chunk.prototype.draw_vis = function(gl, vis_shader, cam)
{
	if(this.is_air || this.vb.empty || !this.in_frustum(cam))
		return;

	var c = Map.vis_base_chunk;
	var pos = new Float32Array([1, 0, 0, 0,
								0, 1, 0, 0,
								0, 0, 1, 0,
								(this.x-c[0])*CHUNK_X, 
								(this.y-c[1])*CHUNK_Y, 
								(this.z-c[2])*CHUNK_Z, 1]);
	
	gl.uniformMatrix4fv(vis_shader.view_mat, false, pos);
	gl.uniform4f(vis_shader.chunk_id, 1, 1, 1, 1);
	
	this.vb.draw_vis(gl, vis_shader);
}

//Releases a chunk and its associated resources
Chunk.prototype.release = function(gl)
{
	this.vb.release(gl);
	delete this.vb;
	delete this.data;
}




