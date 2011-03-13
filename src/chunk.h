#ifndef CHUNK_H
#define CHUNK_H

#include <cassert>
#include <iostream>
#include <cstdlib>
#include <cstdint>

#include <tbb/

#include "constants.h"
#include "network.pb.h"

namespace Game
{
	//The block type
	enum BlockType {
		BlockType_Air,
		BlockType_Stone,
		BlockType_Dirt,
		BlockType_Grass,
		BlockType_Cobblestone,
		BlockType_Wood,
		BlockType_Log,
		BlockType_Water,
		BlockType_Sand
	};
	
	//The transparency data for a block type
	extern const bool BLOCK_TRANSPARENCY[];

	//A block object
	#pragma pack(push)
	#pragma pack(1)	
	struct Block
	{
		std::uint8_t 	type;
		std::uint8_t	state[3];
	};
	#pragma pack(pop)

	//A map coordinate
	struct Coord
	{
		double x, y, z;
		
		Coord();
		Coord(double x_, double y_, double z_);
	};
	
	//A chunk index into the map
	struct ChunkID
	{
		//Chunk coordinates are 21-bit unsigned ints
		std::uint32_t x, y, z;
		
		ChunkID() : x(0), y(0), z(0) {}
		ChunkID(uint32_t x_, uint32_t y_, uint32_t z_) : x(x_), y(y_), z(z_) {}		
	};
	
	
	//A region in the map
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
		
	//Chunk compression/decompression
	uint8_t* compress_chunk(Block* chunk, int stride_x, int stride_xy, int* size);
	void decompress_chunk(Block* chunk, int stride_x, int stride_xy, uint8_t* buffer, int size);
};

#endif

