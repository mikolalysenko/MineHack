
//The chunk worker thread
importScripts(
	'constants.js', 
	'misc.js', 
	'chunk_common.js');

const VERT_SIZE			= 12;

const PACK_X_STRIDE		= 1;
const PACK_Y_STRIDE		= CHUNK_X * 3;
const PACK_Z_STRIDE		= CHUNK_X * CHUNK_Y * 9;

const PACK_X_OFFSET		= CHUNK_X;
const PACK_Y_OFFSET		= CHUNK_Y;
const PACK_Z_OFFSET		= CHUNK_Z;

var
	v_ptr = 0,
	i_ptr = 0,
	t_ptr = 0,
	nv = 0,
	
	vbuffer		= new Array(6*4*CHUNK_SIZE*VERT_SIZE),
	indbuffer	= new Array(6*4*CHUNK_SIZE),
	tindbuffer	= new Array(6*4*CHUNK_SIZE);
	
//Retrieves a pointer into a packed buffer
function get_ptr(j, k)
{
	return PACK_X_OFFSET-1 +
		(PACK_Y_OFFSET+j)*PACK_Y_STRIDE +
		(PACK_Z_OFFSET+k)*PACK_Z_STRIDE;
}

//Calculates ambient occlusion value
function ao_value(s1, s2, c)
{
	s1 = !Transparent[s1];
	s2 = !Transparent[s2];
	c  = !Transparent[c];
	
	if(s1 && s2)
	{
		return 0.25;
	}
	
	return 1.0 - 0.25 * (s1 + s2 + c);
}

//Calculates the light value
function calc_light(
	ox, oy, oz,
	nx, ny, nz,
	s1, s2, c)
{
	var ao;
	
	ao = ao_value(s1, s2, c);
	
	return [ao, ao, ao];
}


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
		light,
		orient = nx * (uy * vz - uz * vy) +
				 ny * (uz * vx - ux * vz) +
				 nz * (ux * vy - uy * vx);

	if(Transparent[block_id])
	{
		if(orient < 0)
		{
			tindbuffer[t_ptr++] = nv;
			tindbuffer[t_ptr++] = nv+1;
			tindbuffer[t_ptr++] = nv+2;
			
			tindbuffer[t_ptr++] = nv;
			tindbuffer[t_ptr++] = nv+2;
			tindbuffer[t_ptr++] = nv+3;

		}
		else
		{
			tindbuffer[t_ptr++] = nv;
			tindbuffer[t_ptr++] = nv+2;
			tindbuffer[t_ptr++] = nv+1;
			
			tindbuffer[t_ptr++] = nv;
			tindbuffer[t_ptr++] = nv+3;
			tindbuffer[t_ptr++] = nv+2;
		}
	}
	else
	{
		if(orient < 0)
		{
			indbuffer[i_ptr++] = nv;
			indbuffer[i_ptr++] = nv+1;
			indbuffer[i_ptr++] = nv+2;

			indbuffer[i_ptr++] = nv;
			indbuffer[i_ptr++] = nv+2;
			indbuffer[i_ptr++] = nv+3;
		}
		else
		{
			indbuffer[i_ptr++] = nv;
			indbuffer[i_ptr++] = nv+2;
			indbuffer[i_ptr++] = nv+1;

			indbuffer[i_ptr++] = nv;
			indbuffer[i_ptr++] = nv+3;
			indbuffer[i_ptr++] = nv+2;
		}
	}

	tc = BlockTexCoords[block_id][dir];
	tx = tc[1] / 16.0;
	ty = tc[0] / 16.0;
	dt = 1.0 / 16.0 - 1.0/256.0;
	
	ox = x - 0.5 + (nx > 0 ? 1 : 0);
	oy = y - 0.5 + (ny > 0 ? 1 : 0);
	oz = z - 0.5 + (nz > 0 ? 1 : 0);

	
	light = calc_light(
		ox, oy, oz,
		nx, ny, nz, 
		ao01, ao10, ao00);
	vbuffer[v_ptr++] = (ox);
	vbuffer[v_ptr++] = (oy);
	vbuffer[v_ptr++] = (oz);
	vbuffer[v_ptr++] = (tx);
	vbuffer[v_ptr++] = (ty+dt);
	vbuffer[v_ptr++] = (nx);
	vbuffer[v_ptr++] = (ny);
	vbuffer[v_ptr++] = (nz);
	vbuffer[v_ptr++] = (light[0]);
	vbuffer[v_ptr++] = (light[1]);
	vbuffer[v_ptr++] = (light[2]);
	vbuffer[v_ptr++] = (0);
	

	light = calc_light(
		ox+ux, oy+uy, oz+uz,
		nx, ny, nz, 
		ao01, ao12, ao02);
	vbuffer[v_ptr++] = (ox + ux);
	vbuffer[v_ptr++] = (oy + uy);
	vbuffer[v_ptr++] = (oz + uz);
	vbuffer[v_ptr++] = (tx);
	vbuffer[v_ptr++] = (ty);
	vbuffer[v_ptr++] = (nx);
	vbuffer[v_ptr++] = (ny);
	vbuffer[v_ptr++] = (nz);
	vbuffer[v_ptr++] = (light[0]);
	vbuffer[v_ptr++] = (light[1]);
	vbuffer[v_ptr++] = (light[2]);
	vbuffer[v_ptr++] = (0);

	light = calc_light(
		ox+ux+vx, oy+uy+vz, oz+uz+vz,
		nx, ny, nz, 
		ao12, ao21, ao22);
	vbuffer[v_ptr++] = (ox + ux + vx);
	vbuffer[v_ptr++] = (oy + uy + vy);
	vbuffer[v_ptr++] = (oz + uz + vz);
	vbuffer[v_ptr++] = (tx+dt);
	vbuffer[v_ptr++] = (ty);
	vbuffer[v_ptr++] = (nx);
	vbuffer[v_ptr++] = (ny);
	vbuffer[v_ptr++] = (nz);
	vbuffer[v_ptr++] = (light[0]);
	vbuffer[v_ptr++] = (light[1]);
	vbuffer[v_ptr++] = (light[2]);
	vbuffer[v_ptr++] = (0);

	light = calc_light(
		ox+vx, oy+vy, oz+vz,
		nx, ny, nz, 
		ao10, ao21, ao20);
	vbuffer[v_ptr++] = (ox + vx);
	vbuffer[v_ptr++] = (oy + vy);
	vbuffer[v_ptr++] = (oz + vz);
	vbuffer[v_ptr++] = (tx+dt);
	vbuffer[v_ptr++] = (ty+dt);
	vbuffer[v_ptr++] = (nx);
	vbuffer[v_ptr++] = (ny);
	vbuffer[v_ptr++] = (nz);
	vbuffer[v_ptr++] = (light[0]);
	vbuffer[v_ptr++] = (light[1]);
	vbuffer[v_ptr++] = (light[2]);
	vbuffer[v_ptr++] = (0);
		
	nv += 4;
}





//Construct vertex buffer for this chunk
// This code makes me want to barf - Mik
function gen_vb(data)
{
	var 
		x=0, y=0, z=0,
	
	//var neighborhood = new Uint32Array(27); (too slow goddammit.  variant arrays even worse.  am forced to do this. hate self.)
		n000=0, n001=0, n002=0,
		n010=0, n011=0, n012=0,
		n020=0, n021=0, n022=0,
		n100=0, n101=0, n102=0,
		n110=0, n111=0, n112=0,
		n120=0, n121=0, n122=0,
		n200=0, n201=0, n202=0,
		n210=0, n211=0, n212=0,
		n220=0, n221=0, n222=0,
	
	//Buffers for scanning	
		p00=0, p01=0, p02=0,
		p10=0, p11=0, p12=0,
		p20=0, p21=0, p22=0;
	
	//Initialize vertex buffer pointers
	v_ptr = 0;
	i_ptr = 0;
	t_ptr = 0;
	nv = 0;
		
	for(z=0; z<CHUNK_Z; ++z)
	for(y=0; y<CHUNK_Y; ++y)
	{
		//Set up neighborhood buffers
		p00 = get_ptr(y-1, z-1);
		p01 = get_ptr(y-1, z);
		p02 = get_ptr(y-1, z+1);
		p10 = get_ptr(y,   z-1);
		p11 = get_ptr(y,   z);
		p12 = get_ptr(y,   z+1);
		p20 = get_ptr(y+1, z-1);
		p21 = get_ptr(y+1, z);
		p22 = get_ptr(y+1, z+1);


		//Read in initial neighborhood
		n100 = data[p00++];
		n101 = data[p01++];
		n102 = data[p02++];
		n110 = data[p10++];
		n111 = data[p11++];
		n112 = data[p12++];
		n120 = data[p20++];
		n121 = data[p21++];
		n122 = data[p22++];
		
		n200 = data[p00++];
		n201 = data[p01++];
		n202 = data[p02++];
		n210 = data[p10++];
		n211 = data[p11++];
		n212 = data[p12++];
		n220 = data[p20++];
		n221 = data[p21++];
		n222 = data[p22++];

		
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
			
			//Read in next neighborhood
			n200 = data[p00++];
			n201 = data[p01++];
			n202 = data[p02++];
			n210 = data[p10++];
			n211 = data[p11++];
			n212 = data[p12++];
			n220 = data[p20++];
			n221 = data[p21++];
			n222 = data[p22++];

			if(n111 == 0)
				continue;
				
				
			if(Transparent[n011] && n011 != n111)
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
			
			if(Transparent[n211] && n211 != n111)
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
			
			if(Transparent[n101] && n101 != n111)
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
			
			
			if(Transparent[n121] && n121 != n111)
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
			

			if(Transparent[n110] && n110 != n111)
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

			if(Transparent[n112] && n112 != n111)
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
	return [vbuffer.slice(0, v_ptr), indbuffer.slice(0, i_ptr), tindbuffer.slice(0, t_ptr)];
}

//Handles the message
self.onmessage = function(ev)
{
	var vbs = gen_vb(ev.data);

	postMessage({x:ev.data.x, y:ev.data.y, z:ev.data.z, verts:vbs[0], ind:vbs[1], tind:vbs[2] });
}


