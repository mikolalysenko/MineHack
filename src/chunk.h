#ifndef CHUNK_H
#define CHUNK_H

#include <cassert>
#include <iostream>
#include <cstdlib>
#include <cstdint>

#include "constants.h"

namespace Game
{
	//The block type
	enum class Block : std::uint8_t
	{
		Air,
		Stone,
		Dirt,
		Grass,
		Cobblestone,
		Wood,
		Log,
		Water,
		Sand,
		
		Nonsense = 0xff
	};
	
	extern const bool BLOCK_TRANSPARENCY[];
	
	//A chunk index into the map
	#pragma pack(push, 1)
	struct ChunkID
	{
		//Chunk coordinates are 21-bit unsigned ints
		std::uint32_t x, y, z;
		
		ChunkID() : x(0), y(0), z(0) {}
		ChunkID(uint32_t x_, uint32_t y_, uint32_t z_) : x(x_), y(y_), z(z_) {}
		
		//Generates a hash for this chunk id (which is actually a unique descriptor in this case)
		std::uint64_t hash() const;
	};
	#pragma pack(pop)
	
	//Face directions
	struct FaceDir
	{
		const static int Left 	= 0;	//-x
		const static int Right	= 1;	//+x
		const static int Bottom	= 2;	//-y
		const static int Top	= 3;	//+y
		const static int Back	= 4;	//-z
		const static int Front	= 5;	//+z
	};
	
	//Chunks are blobs of blocks
	// basic unit the game map/vehicles are composed of
	struct Chunk
	{
		Block 		data[CHUNK_X*CHUNK_Y*CHUNK_Z];
		
		//Block accessor functions
		Block get(int x, int y, int z) const
		{
			assert(	x >= 0 && x < CHUNK_X &&
					y >= 0 && y < CHUNK_Y &&
					z >= 0 && z < CHUNK_Z );
			return data[CHUNK_OFFSET(x, y, z)];
		}
		
		Block set(int x, int y, int z, Block b)
		{
			assert( x >= 0 && x < CHUNK_X &&
					y >= 0 && y < CHUNK_Y &&
					z >= 0 && z < CHUNK_Z );
			return data[CHUNK_OFFSET(x, y, z)] = b;			
		}
		
		//Compress this chunk into the target buffer
		//Returns the length of the encoded chunk, or -1 if it failed
		int compress(void* target, int len);
	};
	
	
	
	//A region in a map
	struct Region
	{
		uint32_t lo[3], hi[3];
		
		Region()
		{
			lo[0] = COORD_MIN_X;
			lo[1] = COORD_MIN_Y;
			lo[2] = COORD_MIN_Z;
			
			hi[0] = COORD_MAX_X;
			hi[1] = COORD_MAX_Y;
			hi[2] = COORD_MAX_Z;
		}
		
		Region(ChunkID const& id)
		{
			lo[0] = id.x << CHUNK_X_S;
			lo[1] = id.y << CHUNK_Y_S;
			lo[2] = id.z << CHUNK_Z_S;

			hi[0] = lo[0] + CHUNK_X;
			hi[1] = lo[1] + CHUNK_Y;
			hi[2] = lo[2] + CHUNK_Z;
		}
		
		Region(	int lo_x, int lo_y, int lo_z,
				int hi_x, int hi_y,	int hi_z )
		{
			lo[0] = lo_x;
			lo[1] = lo_y;
			lo[2] = lo_z;
			
			hi[0] = hi_x;
			hi[1] = hi_y;
			hi[2] = hi_z;
		}
	};
};

#endif

