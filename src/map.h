#ifndef MAP_H
#define MAP_H

#include <cstdint>

#define CHUNK_X 32
#define CHUNK_Y 32
#define CHUNK_Z 32

namespace MapData
{
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

	struct ChunkIndex
	{
		std::int32_t x, y, z;
	};

	struct Chunk
	{
		ChunkIndex idx;
		Block data[CHUNK_X+2][CHUNK_Y+2][CHUNK_Z+2];
		
		//Compress this chunk into the target buffer
		//Returns the length of the encoded chunk, or -1 if it failed
		int compress(void* target, size_t len);
	};
	
	struct Map
	{
		//Map constructor
		Map();
		
		//
		Chunk* get_chunk(ChunkIndex);
	};
};


#endif

