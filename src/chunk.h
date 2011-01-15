#ifndef CHUNK_H
#define CHUNK_H

#include <cassert>
#include <iostream>
#include <cstdlib>
#include <cstdint>

//Chunk dimensions

#define CHUNK_X_S			4
#define CHUNK_Y_S			4
#define CHUNK_Z_S			4

#define CHUNK_X				(1<<CHUNK_X_S)
#define CHUNK_Y				(1<<CHUNK_Y_S)
#define CHUNK_Z				(1<<CHUNK_Z_S)

#define CHUNK_X_MASK		(CHUNK_X-1)
#define CHUNK_Y_MASK		(CHUNK_Y-1)
#define CHUNK_Z_MASK		(CHUNK_Z-1)

//Index within a chunk
#define CHUNK_OFFSET(X,Y,Z)	((X)+((Y)<<CHUNK_X_S)+((Z)<<(CHUNK_X_S+CHUNK_Y_S)))

//Maximum length for a compressed chunk
#define MAX_CHUNK_BUFFER_LEN	(CHUNK_X*CHUNK_Y*CHUNK_Z*2)

//Number of bits for a chunk index
#define CHUNK_IDX_S			21ULL

//Size of max chunk index
#define CHUNK_IDX_MAX		(1<<CHUNK_IDX_S)

#define CHUNK_IDX_MASK		(CHUNK_IDX_MAX - 1)

//Coordinate indexing stuff
#define COORD_MIN_X			0
#define COORD_MIN_Y			0
#define COORD_MIN_Z			0

#define COORD_MAX_X			(CHUNK_X + (CHUNK_IDX_MAX<<CHUNK_X_S))
#define COORD_MAX_Y			(CHUNK_Y + (CHUNK_IDX_MAX<<CHUNK_Y_S))
#define COORD_MAX_Z			(CHUNK_Z + (CHUNK_IDX_MAX<<CHUNK_Z_S))

//Converts a coordinate into a chunk index
#define COORD2CHUNKID(X,Y,Z)	(Game::ChunkID( ((X)>>(CHUNK_X_S)) & CHUNK_IDX_MASK, \
												((Y)>>(CHUNK_Y_S)) & CHUNK_IDX_MASK, \
												((Z)>>(CHUNK_Z_S)) & CHUNK_IDX_MASK ) )

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
		Sand
	};

	//A chunk index into the map
	struct ChunkID
	{
		//Chunk coordinates are 21-bit unsigned ints
		std::uint32_t x, y, z;
		
		ChunkID() : x(0), y(0), z(0) {}
		ChunkID(uint32_t x_, uint32_t y_, uint32_t z_) : x(x_), y(y_), z(z_) {}
		
		//Generates a hash for this chunk id (which is actually a unique descriptor in this case)
		std::uint64_t hash() const;
	};

	//Chunks are blobs of blocks
	// basic unit the game map/vehicles are composed of
	struct Chunk
	{
		Block data[CHUNK_X*CHUNK_Y*CHUNK_Z];
		
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
		int compress(void* target, size_t len);
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

