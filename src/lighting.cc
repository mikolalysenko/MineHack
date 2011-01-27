#include "misc.h"
#include "chunk.h"
#include "lighting.h"

using namespace std;
using namespace Game;

void LightCalculator::compute_light(ChunkID const& lo, ChunkID const& hi, int niter)
{
	int n, x, y, z;
	int pad_x = 1 + (niter/CHUNK_X),
		pad_y = 1 + (niter/CHUNK_Y),
		pad_z = 1 + (niter/CHUNK_Z);
		
	int lx = lo.x - pad_x,	ux = hi.x + pad_x,
		ly = lo.y - pad_y,	uy = hi.y + pad_y,
		lz = lo.z - pad_z,	uz = hi.z + pad_z;
	
	int sx = ux - lx + 1,
		sy = uy - ly + 1,
		sz = uz - lz + 1;
		
		
	Chunk* grid = (Chunk*)malloc(sizeof(Chunk) * sx * sy * sz);
	ScopeFree G(grid);
	
	for(x=0; x<sx; ++x)
	for(y=0; y<sy; ++y)
	for(z=0; z<sz; ++z)
	{
		game_map->get_chunk(ChunkID(x+lx,y+ly,z+lz), &grid[x+sx*(y+sy*z)]);
	}
	
	
	#define GRID_IDX(X,Y,Z) 	(((X)>>CHUNK_X_S)+sx*(((Y)>>CHUNK_Y_S)+sy*((Z)>>CHUNK_Z_S)))
	#define CELL_IDX(X,Y,Z)		CHUNK_OFFSET( (X)&CHUNK_X_MASK, (Y)&CHUNK_Y_MASK, (Z)&CHUNK_Z_MASK )
	
	
	LightVal *buffer = (LightVal*)malloc(sizeof(LightVal) * sy * sz);
	ScopeFree H(buffer);
	
	for(n=1; n<=niter; ++n)
	{
		for(x=n; x<sx*CHUNK_X-n; ++x)
		{
			for(y=n; y<sy*CHUNK_Y-n; ++y)
			for(z=n; z<sz*CHUNK_Z-n; ++z)
			{		
				LightVal	center	= grid[GRID_IDX(x,y,z)].light[CELL_IDX(x,y,z)],
							left	= grid[GRID_IDX(x-1,y,z)].light[CELL_IDX(x-1,y,z)],
							right	= grid[GRID_IDX(x+1,y,z)].light[CELL_IDX(x+1,y,z)],
							bottom	= grid[GRID_IDX(x,y-1,z)].light[CELL_IDX(x,y-1,z)],
							top		= grid[GRID_IDX(x,y+1,z)].light[CELL_IDX(x,y+1,z)],
							front	= grid[GRID_IDX(x,y,z-1)].light[CELL_IDX(x,y,z-1)],
							back	= grid[GRID_IDX(x,y,z+1)].light[CELL_IDX(x,y,z+1)];

				int b = (int)grid[GRID_IDX(x,y,z)].data[CELL_IDX(x,y,z)];
			
				float	reflect = BLOCK_REFLECTANCE[b],
						transp	= BLOCK_TRANSMISSION[b],
						scatter	= BLOCK_SCATTER[b],
						emiss	= BLOCK_EMISSIVITY[b];
					
				LightVal	res;
			
				float s, s_total = 
					left.face[FaceDir::Right] + right.face[FaceDir::Left] +
					top.face[FaceDir::Bottom] + bottom.face[FaceDir::Top] +
					front.face[FaceDir::Back] + back.face[FaceDir::Front];		
			
				s = scatter * (s_total - left.face[FaceDir::Right] - right.face[FaceDir::Right]);
				res.face[FaceDir::Left]		= emiss + s + reflect * left.face[FaceDir::Right] + transp * right.face[FaceDir::Left];
				res.face[FaceDir::Right]	= emiss + s + reflect * right.face[FaceDir::Left] + transp * left.face[FaceDir::Right];
			
				s = scatter * (s_total - top.face[FaceDir::Bottom] - bottom.face[FaceDir::Top]);
				res.face[FaceDir::Top]		= emiss + s + reflect * top.face[FaceDir::Bottom] + transp * bottom.face[FaceDir::Top];
				res.face[FaceDir::Bottom]	= emiss + s + reflect * bottom.face[FaceDir::Top] + transp * top.face[FaceDir::Bottom];
			
				s = scatter * (s_total - front.face[FaceDir::Back] - back.face[FaceDir::Front]);
				res.face[FaceDir::Front]	= emiss + s + reflect * front.face[FaceDir::Back] + transp * back.face[FaceDir::Front];
				res.face[FaceDir::Back]		= emiss + s + reflect * back.face[FaceDir::Front] + transp * front.face[FaceDir::Back];
			
				buffer[y + sy * z] = res;
			}
			
			for(y=n; y<sy*CHUNK_Y-n; ++y)
			for(z=n; z<sz*CHUNK_Z-n; ++z)
			{
				grid[GRID_IDX(x,y,z)].light[CELL_IDX(x,y,z)] = buffer[y + sy * z];
			}
		}
	}
	
	#undef CHUNK_IDX
	#undef GRID_IDX
	
	//Write result back
	for(x = lo.x; x<=hi.x; ++x)
	for(y = lo.y; y<=hi.y; ++y)
	for(z = lo.z; z<=hi.z; ++z)
	{
		game_map->update_lighting(ChunkID(x, y, z), grid[x + sx * (y + sy * z)].light);
	}
}
