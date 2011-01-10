#ifndef CHUNK_H
#define CHUNK_H

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

	struct ChunkId
	{
		//Chunk coordinates are 21-bit unsigned ints
		std::uint32_t x, y, z;
		
		ChunkId() : x(0), y(0), z(0) {}
		ChunkId(uint32_t x_, uint32_t y_, uint32_t z_) : x(x_), y(y_), z(z_) {}
		
		//Generates a hash for this chunk id (which is actually a unique descriptor in this case)
		std::uint64_t hash() const;
	};

	struct Chunk
	{
		ChunkId idx;
		bool dirty;  //If set, this chunk has been modified
		Block data[CHUNK_X][CHUNK_Y][CHUNK_Z];
		
		//Compress this chunk into the target buffer
		//Returns the length of the encoded chunk, or -1 if it failed
		int compress(void* target, size_t len);
		
		//Decompresses a chunk from a buffer
		static Chunk* decompress(void* buffer, size_t len);
	};
	
};

#endif

