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


//The chunk data type
function Chunk(x, y, z)
{
	//Set chunk data
	this.data = new Uint8Array(CHUNK_SIZE);
	
	//Set position
	this.x = x;
	this.y = y;
	this.z = z;
	
	this.pending = true;
	
	this.vb = null;
	this.ib = null;
	this.tib = null;
}

function frustum_test(m, cx, cy, cz)
{
	var vx = (cx)*CHUNK_X,
		vy = (cy)*CHUNK_Y,
		vz = (cz)*CHUNK_Z,
		qx, qy, qz, 
		dx, dy, dz,
		in_p = 0, w, x, y, z;
	
	
	for(dx=-1; dx<=CHUNK_X; dx+=CHUNK_X+1)
	for(dy=-1; dy<=CHUNK_Y; dy+=CHUNK_Y+1)
	for(dz=-1; dz<=CHUNK_Z; dz+=CHUNK_Z+1)	
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
		if(z >=  0) in_p |= 32;
		if(in_p == 63)	return true;
	}

	return false;

}

//Sets the block type and updates vertex buffers as needed
Chunk.prototype.set_block = function(x, y, z, b)
{
	this.data[x + (y<<CHUNK_X_S) + (z<<CHUNK_XY_S)] = b;
}

//The map data structure
var Map =
{
	index			: {},	//The chunk index
	
	max_chunks		: 80000,	//Maximum number of chunks to load (not used yet)
	chunk_count 	: 0,		//Number of loaded chunks
	chunk_radius	: 3,		//These chunks are always fetched.
	chunk_init_radius	: 4,		//Initially fetched chunks
	num_pending_chunks	: 0,
	
	
	//If set, then we draw the debug info for the chunk
	show_debug		: false,
	
	//Visibility stuff
	vis_angle		: 0,
	vis_width		: 128,
	vis_height		: 128,
	vis_fov			: Math.PI * 3.0 / 4.0,
	vis_state		: 0,	
	vis_bounds		: [ [1, 6],
						[7, 10],
						[11, 13],
						[14, 15],
						[16, 16] ]
	
};

//Hash look up in map
Map.lookup_chunk = function(x, y, z)
{
	return Map.index[x + ":" + y + ":" + z];
}

//Adds a new chunk
Map.add_chunk = function(x, y, z)
{
	var str = x + ":" + y + ":" + z,
		chunk = Map.index[str];

	if(chunk)
	{
		return chunk;
	}
	
	chunk = new Chunk(x, y, z);
	Map.index[str] = chunk
	return chunk;
}

//Returns the block type for the give position
Map.get_block = function(x, y, z)
{
	var cx = (x >> CHUNK_X_S), 
		cy = (y >> CHUNK_Y_S), 
		cz = (z >> CHUNK_Z_S);
	var c = Map.lookup_chunk(cx, cy, cz);		
	if(!c)
		return 1;
	
	var bx = (x & CHUNK_X_MASK), 
		by = (y & CHUNK_Y_MASK), 
		bz = (z & CHUNK_Z_MASK);
	return c.data[bx + (by << CHUNK_X_S) + (bz << CHUNK_XY_S)];
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




