const LEFT		= 0;
const RIGHT		= 1;
const BOTTOM	= 2;
const TOP		= 3;
const FRONT		= 4;
const BACK		= 5;

//A light block computes lighting for 2x2x2 chunks
var LightBlock = function(x, y, z)
{
	this.x = x;
	this.y = y;
	this.z = z;
	this.light = new Uint8Array(CHUNK_SIZE * 2 * 2 * 2 * 6);
}

LightBlock.prototype.compute = function()
{
	var n, x, y, z, i;

	//Initialize buffer
	for(z=0; z<4; ++z)
	for(y=0; y<4; ++y)
	for(x=0; x<4; ++x)
	{
		LightEngine.set_buf_chunk(
			this.x+x-1, this.y+y-1, this.z+z-1,
			x, y, z);
	}
	
	
	//Do lighting calculation
	var geom = LightEngine.geom;

	
	for(n=1; n<=16; ++n)
	{
		var s = LightEngine.buf[0],
			t = LightEngine.buf[1],
			tr = 0;
			
		//FIXME: BOundary conditions are wrong	
		
		for(z=n; z<4*CHUNK_Z-n; ++z)
		for(y=n; y<4*CHUNK_Y-n; ++y)
		for(x=n; x<4*CHUNK_X-n; ++x)
		{
			var idx = x + y*CHUNK_X*4 + z*CHUNK_X*CHUNK_Y*16;
			
			var mat = BlockMaterials[geom[idx]];
			
			var transmit	= 1, //mat[0],
				scatter		= 0, //mat[1],
				reflect		= 0, //mat[2],
				emiss		= 0; //mat[3];
			
			idx *= 6;
			var oleft 	= s[idx + LEFT],
				oright	= s[idx + RIGHT],
				otop	= s[idx + TOP],
				obottom = s[idx + BOTTOM],
				ofront	= s[idx + FRONT],
				oback	= s[idx + BACK];
				
			var ileft	= s[((x-1) + y*CHUNK_X*4 + z*CHUNK_X*CHUNK_Y*16)*6 + RIGHT],
				iright	= s[((x+1) + y*CHUNK_X*4 + z*CHUNK_X*CHUNK_Y*16)*6 + LEFT],
				ibottom	= s[(x + (y-1)*CHUNK_X*4 + z*CHUNK_X*CHUNK_Y*16)*6 + TOP],
				itop	= s[(x + (y+1)*CHUNK_X*4 + z*CHUNK_X*CHUNK_Y*16)*6 + BOTTOM],
				ifront	= s[(x + y*CHUNK_X*4 + (z-1)*CHUNK_X*CHUNK_Y*16)*6 + BACK],
				iback	= s[(x + y*CHUNK_X*4 + (z+1)*CHUNK_X*CHUNK_Y*16)*6 + FRONT];

				
			var sc, sc_total = ileft + iright + itop + ibottom + ifront + iback;			
			sc = scatter * (sc_total - ileft - iright);
			t[idx + LEFT]	= Math.min(0xff, emiss + sc + reflect * ileft	+ transmit * iright);
			t[idx + RIGHT]	= Math.min(0xff, emiss + sc + reflect * iright	+ transmit * ileft);

			sc = scatter * (sc_total - itop - ibottom);
			t[idx + TOP]	= Math.min(0xff, emiss + sc + reflect * itop	+ transmit * ibottom);
			//t[idx + BOTTOM]	= Math.min(0xff, emiss + sc + reflect * ibottom	+ transmit * itop);
			t[idx + BOTTOM] = itop;

			sc = scatter * (sc_total - ifront - iback);
			t[idx + FRONT]	= Math.min(0xff, emiss + sc + reflect * ifront	+ transmit * iback);
			t[idx + BACK]	= Math.min(0xff, emiss + sc + reflect * iback	+ transmit * ifront);
			
			if(z > CHUNK_Z && z <3*CHUNK_Z &&
				x > CHUNK_X && x < 3*CHUNK_X &&
				y > CHUNK_Y && y < 3*CHUNK_Y )
			{
				for(i=0; i<6; ++i)
					tr += t[idx+i];
			}
		}
	
		alert("Total radiance = " + tr);
	
		LightEngine.swap_buf();
	}

	//Save results back to light buffer
	var idx = 0, buf = LightEngine.buf[0], total_r = 0;
	for(z=0; z<2*CHUNK_Z; ++z)
	for(y=0; y<2*CHUNK_Y; ++y)
	for(x=0; x<2*CHUNK_X; ++x)
	{
		var off = ((x+CHUNK_X) + (y+CHUNK_Y)*4*CHUNK_X + (z+CHUNK_Z)*16*CHUNK_X*CHUNK_Y) * 6;
		for(i=0; i<6; i++)
		{
			this.light[idx++] = buf[off++];
			total_r += this.light[idx - 1];
		}
	}
	
	alert(total_r);
	
	//Update chunks
	for(z=0; z<2; ++z)
	for(y=0; y<2; ++y)
	for(x=0; x<2; ++x)
	{
		var c = Map.lookup_chunk(x+this.x, y+this.y, z+this.z);
		
		if(c)
		{
			c.vb.set_dirty();
		}
	}
}

//The light engine indexes a collection of blocks
var LightEngine = 
{
	index : {},
	buf : [ new Uint8Array(CHUNK_SIZE * 4 * 4 * 4 * 6), 
			new Uint8Array(CHUNK_SIZE * 4 * 4 * 4 * 6) ],
			
	geom : new Uint8Array(CHUNK_SIZE * 4 * 4 * 4),
	
	pending : [],
	pending_map : {}
};


//Used for setting boundary conditions
LightEngine.set_buf_chunk = function(
	sx, sy, sz,		//Source chunk ID
	dx, dy, dz)		//Destination chunk offset
{
	var g = LightEngine.geom;
	var d = LightEngine.buf[0];
	var doff = dx * CHUNK_X + dy*CHUNK_X*CHUNK_Y*4 + dz * CHUNK_X * CHUNK_Y * CHUNK_Z * 16;

	var c = Map.lookup_chunk(sx, sy, sz);
	

	var s = LightEngine.lookup_block(sx&~1, sy&~1, sz&~1);
	if(s)		
		s = s.light;
	
	sx &= 1; sy &= 1; sz &= 1;
	var soff = (sx*CHUNK_X + sy*CHUNK_X*CHUNK_Y*2 + sz*CHUNK_X*CHUNK_Y*CHUNK_Z*4) * 6;
	
	
	for(var z=0; z<CHUNK_Z; ++z)
	for(var y=0; y<CHUNK_Y; ++y)
	for(var x=0; x<CHUNK_X; ++x)
	{
		var idoff = doff + x + y * CHUNK_X * 4 + z * CHUNK_X * CHUNK_Y * 16;
		var isoff = soff + (x + y * CHUNK_X * 2 + z * CHUNK_X * CHUNK_Y * 4) * 6;
		
		if(c)
		{
			g[idoff] = c[x + y*CHUNK_X + z*CHUNK_X*CHUNK_Y];
		}
		else
		{
			g[idoff] = 0;
		}
		
		idoff *= 6;
		if(c && c.is_air)
		{
			for(var i=0; i<6; ++i)
				d[idoff + i] = 0;
			d[idoff + BOTTOM] = 255;
		}
		else if(s)
		{
			for(var i=0; i<6; ++i)
				d[idoff + i] = s[isoff + i];
		}
		else
		{
			for(var i=0; i<6; ++i)
				d[idoff + i] = 0;
		}
	}
}

LightEngine.swap_buf = function()
{
	var tmp = LightEngine.buf[0];
	LightEngine.buf[0] = LightEngine.buf[1];
	LightEngine.buf[1] = tmp;
}

LightEngine.lookup_block = function(x, y, z)
{
	x &= ~1; y &= ~1; z &= ~1;
	var str = x + ":" + y + ":" + z;
	return LightEngine.index[str];
}

LightEngine.create_block = function(x, y, z)
{
	x &= ~1; y &= ~1; z &= ~1;
	var str = x + ":" + y + ":" + z;
	if(str in LightEngine.index)
		return LightEngine.index[str];
	var b = new LightBlock(x, y, z);
	LightEngine.index[str] = b;
	return b;
}

LightEngine.add_job = function(x, y, z)
{
	x &= ~1; y &= ~1; z &= ~1;
	var str = x + ":" + y + ":" + z;

	if(str in LightEngine.pending_map || !(str in LightEngine.index))
		return;

	LightEngine.pending_map[str] = true;
	LightEngine.pending.push( [ x, y, z ]);
}

LightEngine.do_job = function()
{
	if(LightEngine.pending.length == 0)
		return;
		
	//Get job
	var job = LightEngine.pending[0];
	var str = job[0] + ":" + job[1] + ":" + job[2];
	
	//Remove job
	LightEngine.pending = LightEngine.pending.slice(0);
	delete LightEngine.pending_map[str];
	
	//Execute
	LightEngine.index[str].compute();
}

