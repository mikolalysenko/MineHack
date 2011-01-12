#ifndef CHUNK_H
#define CHUNK_H

#include <cassert>
#include <iostream>
#include <cstdlib>
#include <cstdint>

#define CHUNK_X 32
#define CHUNK_Y 32
#define CHUNK_Z 32

#define CHUNK_MAX_X		(1<<21)
#define CHUNK_MAX_Y		(1<<21)
#define CHUNK_MAX_Z		(1<<21)


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
		Log
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
			return data[x + (y<<5) + (z<<10)];
		}
		
		Block set(int x, int y, int z, Block b)
		{
			assert( x >= 0 && x < CHUNK_X &&
					y >= 0 && y < CHUNK_Y &&
					z >= 0 && z < CHUNK_Z );
			return data[x + (y<<5) + (z<<10)] = b;			
		}
		
		//Compress this chunk into the target buffer
		//Returns the length of the encoded chunk, or -1 if it failed
		int compress(void* target, size_t len);
	};
	
};

#endif

