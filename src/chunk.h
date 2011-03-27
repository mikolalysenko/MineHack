#ifndef CHUNK_H
#define CHUNK_H

#include <stdint.h>

#include <map>
#include <vector>

#include <tbb/tbb_allocator.h>

#include "constants.h"
#include "network.pb.h"

namespace Game
{
	//Forward declarations
	class Block;
	class Coord;
	class ChunkID;

	//Block types
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
	//FIXME: Replace this with a simple rule
	extern const bool BLOCK_TRANSPARENCY[];

	//Number of state bytes per block
	//FIXME: Replace this with a bit flag...
	extern const int BLOCK_STATE_BYTES[];

	//A block object (ie one voxel inside the map)
	#pragma pack(push)
	#pragma pack(1)	
	struct Block
	{
		uint32_t	int_val;
		
		Block() : int_val(0) {}
		
		Block(uint8_t t, uint8_t s0 = 0, uint8_t s1=0, uint8_t s2=0) :
			int_val(t | (s0<<8) | (s1<<16) | (s2<<24) )
		{ }
		
		Block(uint8_t t, uint8_t* s) : int_val(t)
		{
			for(int i=1; i<=BLOCK_STATE_BYTES[t]; ++i)
				int_val |= (uint32_t)s[i] << (8*i);
		}
		
		Block(const Block& other)
		{
			int_val = other.int_val;
		}
		
		Block& operator=(const Block& other)
		{
			int_val = other.int_val;
			return *this;
		}
		
		Block& operator=(uint8_t t)
		{
			int_val = t;
			return *this;
		}
		
		bool operator==(const Block& other) const
		{
			return int_val == other.int_val;
		}
		
		bool operator!=(const Block& other) const
		{
			return int_val != other.int_val;
		}
		
		//State accessors
		uint8_t type() const		{ return int_val&0xff; }
		bool transparent() const	{ return BLOCK_TRANSPARENCY[type()]; }
		int state_bytes() const		{ return BLOCK_STATE_BYTES[type()]; }
		int state(int i) const		{ return (int_val>>(8*(i+1))) & 0xff; }
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
		
		//Chunks must be locked wrt to this order: y-z-x, low to high.
		bool operator<(const ChunkID& other) const
		{
			if(y != other.y)	return y < other.y;
			if(z != other.z)	return z < other.z;
			return x < other.x;
		}
	};
	
	//Hash comparison for chunk IDs
	struct ChunkIDHashCompare
	{
		bool equal(const ChunkID& A, const ChunkID& B) const { return A == B; }
		size_t hash(const ChunkID& chunk_id) const;
		size_t operator()(ChunkID const& chunk_id) const { return hash(chunk_id); }
	};
	

	//A compressed chunk record
	struct ChunkBuffer
	{
		typedef std::map<int, Block, std::less<int>, tbb::tbb_allocator< std::pair<const int, Block> > >	interval_tree_t;
	
		ChunkBuffer() : is_empty(false), valid_flag(false), timestamp(1) {}
	
		//Block accessors
		Block get_block(int x, int y, int z) const;
		bool set_block(Block b, int x, int y, int z, uint64_t t);
		
		//Buffer decoding/access
		void compress_chunk(Block* chunk, int stride_x=CHUNK_X, int stride_xz=CHUNK_X*CHUNK_Z);
		void decompress_chunk(Block* chunk, int stride_x=CHUNK_X, int stride_xz=CHUNK_X*CHUNK_Z) const;
		
		//Protocol buffer interface
		void cache_protocol_buffer_data();
		void parse_from_protocol_buffer(Network::Chunk const&);
		bool serialize_to_protocol_buffer(Network::Chunk&) const;
		
		//Time stamp accessors
		uint64_t last_modified() const { return timestamp; }
		uint64_t set_last_modified(uint64_t t) { return timestamp = t; }
		
		//For surface chunks
		bool empty_surface() const { return is_empty; }
		bool set_empty_surface(bool b) { return is_empty = b; }
		
		//The internal representation of the interval tree
		interval_tree_t interval_tree() const { return intervals; }
		bool equals(interval_tree_t const& tree) const;
		
		void set_valid(bool nv) { valid_flag = nv; }
		bool valid() const { return valid_flag; }
		
	private:
		//For surface chunks, checks if the chunk is empty
		bool is_empty, valid_flag;
	
		//Last time this chunk buffer was updated
		uint64_t	timestamp;
		
		//The interval tree
		
		interval_tree_t		intervals;
		
		//Protocol buffer data
		std::vector<uint8_t, tbb::tbb_allocator< uint8_t > >		pbuffer_data;
	};	
};

#endif

