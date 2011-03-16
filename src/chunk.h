#ifndef CHUNK_H
#define CHUNK_H

#include <stdint.h>

#include "constants.h"
#include "network.pb.h"

namespace Game
{
	//Forward declarations
	class Block;
	class Coord;
	class ChunkID;

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

	//Number of state bytes per block
	extern const int BLOCK_STATE_BYTES[];

	//A block object
	#pragma pack(push)
	#pragma pack(1)	
	struct Block
	{
		uint8_t 	type;
		uint8_t	state[3];
		
		Block() : type(BlockType_Air) { }
		Block(uint8_t t) : type(t) { }
		Block(uint8_t t, uint8_t s0, uint8_t s1=0, uint8_t s2=0) : type(t)
		{
			state[0] = s0;
			state[1] = s1;
			state[2] = s2;
		}
		Block(uint8_t t, uint8_t* s) : type(t)
		{
			for(int i=0; i<BLOCK_STATE_BYTES[t]; ++i)
				state[i] = s[i];
		}
		
		Block(const Block& other) : type(other.type)
		{
			for(int i=0; i<3; ++i)
				state[i] = other.state[i];
		}
		
		Block& operator=(const Block& other)
		{
			type = other.type;
			for(int i=0; i<3; ++i)
				state[i] = other.state[i];
			return *this;
		}
		
		Block& operator=(uint8_t t)
		{
			type = t;
			return *this;
		}
		
		bool operator==(const Block& other) const;
		
		//Encodes a block as a uint32
		uint32_t to_uint() const
		{
			uint32_t result = type;
			for(int i=0; i<BLOCK_STATE_BYTES[type]; ++i)
			{
				result |= (state[i]<<((i+1)*8));
			}
			return result;
		}
	};
	#pragma pack(pop)

	//A map coordinate
	
	struct Coord
	{
		double x, y, z;
		
		Coord() : x(ORIGIN_X), y(ORIGIN_Y), z(ORIGIN_Z) {}
		Coord(const Coord& c) : x(c.x), y(c.y), z(c.z) {}
		Coord(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
		
		Coord& operator=(Coord const& other)
		{
			x = other.x;
			y = other.y;
			z = other.z;
			return *this;
		}
	};
	
	//A chunk index into the map
	struct ChunkID
	{
		//Chunk coordinates are 21-bit unsigned ints
		uint32_t x, y, z;
		
		ChunkID() : x(0), y(0), z(0) {}
		ChunkID(uint32_t x_, uint32_t y_, uint32_t z_) : x(x_), y(y_), z(z_) {}
		ChunkID(const Coord& c) : x(c.x/CHUNK_X), y(c.y/CHUNK_Y), z(c.z/CHUNK_Z) {}
		ChunkID(const ChunkID& other) : x(other.x), y(other.y), z(other.z) {}
		ChunkID& operator=(const ChunkID& other)
		{
			x = other.x;
			y = other.y;
			z = other.z;
			return *this;
		}
		
		bool operator==(const ChunkID& other) const
		{
			return x == other.x && y == other.y && z == other.z;
		}
	};
	
	//Hash comparison for chunk IDs
	struct ChunkIDHashCompare
	{
		bool equal(const ChunkID& A, const ChunkID& B) const { return A == B; }
		size_t hash(const ChunkID& chunk_id) const;
		size_t operator()(ChunkID const& chunk_id) const { return hash(chunk_id); }
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
	
	
	//A compressed chunk record
	struct ChunkBuffer
	{
		int 		size;
		uint64_t	last_modified;
		uint8_t* 	data;
	};
		
	//Chunk compression/decompression
	ChunkBuffer compress_chunk(Block* chunk, int stride_x=CHUNK_X, int stride_xz=CHUNK_X*CHUNK_Z);
	void decompress_chunk(ChunkBuffer const&, Block* chunk, int stride_x=CHUNK_X, int stride_xz=CHUNK_X*CHUNK_Z);
};

#endif

