#include <stdint.h>
#include <cstdlib>

#include <tbb/task.h>

#include "constants.h"
#include "misc.h"
#include "chunk.h"

using namespace std;
using namespace tbb;

#define CHUNK_DEBUG 1

#ifndef CHUNK_DEBUG
#define DEBUG_PRINTF(...)
#else
#define DEBUG_PRINTF(...)  fprintf(stderr,__VA_ARGS__)
#endif

namespace Game
{

//Block data
const bool BLOCK_TRANSPARENCY[] =
{
	true,		//Air
	false,		//Stone
	false,		//Dirt
	false,		//Grass
	false,		//Cobblestone
	false,		//Wood
	false,		//Log
	true,		//Water
	false		//Sand
};

//Block state bytes (can be from 0 to 3)
const int BLOCK_STATE_BYTES[] =
{
	0,		//Air
	0,		//Stone
	0,		//Dirt
	0,		//Grass
	0,		//Cobblestone
	0,		//Wood
	0,		//Log
	0,		//Water
	0		//Sand
};

//Hashes chunk indices
size_t ChunkIDHashCompare::hash(const ChunkID& chunk_id) const
{
	uint64_t h = 0ULL;
	for(uint64_t i=0ULL; i<64ULL; i+=3)
	{
		h |= (((uint64_t)chunk_id.x&1ULL)<<i) +
			 (((uint64_t)chunk_id.y&1ULL)<<(i+1)) +
			 (((uint64_t)chunk_id.z&1ULL)<<(i+2));
	}
	return (size_t)h;
}

//Sets a block in a chunk buffer
// This is complicated -- Mik
//FIXME: Not tested
bool ChunkBuffer::set_block(Block b, int x, int y, int z, uint64_t t)
{
	int offset = x + z * CHUNK_X + y * CHUNK_X * CHUNK_Z;
	
	auto iter = intervals.lower_bound(offset);
	auto c = iter->second;
	if(b == c)
		return false;
		
	timestamp = t;
		
	//Mark protocol buffer data for update
	pbuffer_data.clear();
	
	//Get the range of the interval
	int l = iter->first, r = CHUNK_X*CHUNK_Y*CHUNK_Z;
	auto next = iter;
	++next;
	if(next != intervals.end())
		r = next->first;
	
	if(l == offset)
	{
		//Case: Insert at start of interval
		
		if(iter == intervals.begin() ||
			intervals.lower_bound(l-1)->second != b)
		{
			//Case: Prev interval distinct
			if(r - l == 1)
			{
				//Case : Interval length == 1
				
				if(next != intervals.end() && b == next->second)
				{
					//Case : next interval same
					//
					//Before:       aaaaa c bbbbbbb
					//					  ^
					//					  b
					//					 
					//After:        aaaaa bbbbbbbb
					//
					
					iter->second = b;
					intervals.erase(next);
				}
				else
				{
					//Case : next interval distinct
					//
					//Before:       aaaaa c ddddd
					//					  ^
					//					  b
					//					 
					//After:        aaaaa b ddddd
					//
					
					iter->second = b;
				}
			}
			else
			{
				//Case : Interval length > 1
				//
				//Before:       aaaaa ccccc
				//					  ^
				//					  b
				//					 
				//After:        aaaaa b cccc
				//
				
				
				iter->second = b;
				intervals.insert(make_pair(l+1, c));
			}
		}
		else if(r - l == 1)
		{
			//Case : Prev interval same, interval length == 1
			if(next != intervals.end() && b == next->second)
			{
				//Case : next interval same
				//
				//Before:       bbbbb c bbbbb
				//					  ^
				//					  b
				//					 
				//After:        bbbbbbbbbbb
				//
				
				intervals.erase(next);
				intervals.erase(intervals.lower_bound(offset));
			}
			else
			{
				//Case : next interval distinct
				//
				//Before:       bbbbb c dddddd
				//					  ^
				//					  b
				//					 
				//After:        bbbbbb dddddd
				//

				intervals.erase(iter);
			}
		}
		else
		{
			//Case : Prev interval same, interval length > 1
			//
			//Before:       bbbbb cccccc
			//					  ^
			//					  b
			//					 
			//After:        bbbbbb ccccc
			//
			
			intervals.erase(iter);
			intervals.insert(make_pair(offset+1, c));
		}
	}
	else if(r-1 == offset)
	{
		//Case: insert at end of interval, interval length is > 1
		
		if(next == intervals.end() || next->second == b)
		{
			//Case : Next interval distinct
			//
			//Before:       ccccccc ddddddd
			//					  ^
			//					  b
			//					 
			//After:        cccccc b ddddddd
			//

			intervals.insert(make_pair(offset, b));
		}
		else
		{
			//Case : Next interval same
			//
			//Before:       ccccccc bbbbbb
			//					  ^
			//					  b
			//					 
			//After:        cccccc bbbbbbb
			//

			intervals.erase(next);
			intervals.insert(make_pair(offset, b));
		}
	}
	else
	{
		//Case:  Insert in middle of interval
		//
		//Before:       ccccccccccccc
		//					  ^
		//					  b
		//					 
		//After:        ccccc b ccccccc
		//

		intervals.insert(make_pair(offset, b));
		intervals.insert(make_pair(offset+1, c));
	}
	return true;
}


Block ChunkBuffer::get_block(int x, int y, int z) const
{
	int o = x + z * CHUNK_X + y * CHUNK_X * CHUNK_Z;

	auto iter = intervals.lower_bound(o);
	
	//It says lower_bound, but it gives you an upper_bound.  Calling upper_bound gives you nonsense.
	//What is this world coming too?  (I may be using this wrong...)
	if(iter == intervals.end())
	{
		if(iter == intervals.begin())
			return Block(BlockType_Air);
		--iter;
	}
	else if(iter->first != o)
		--iter;
	
	return iter->second;
}


//Chunk compression
void ChunkBuffer::compress_chunk(Block* data, int stride_x, int stride_xz)
{
	intervals.clear();
	pbuffer_data.clear();
	
	auto data_ptr = data;
	
	const int dx = 1;
	const int dy = stride_xz - (stride_x * (CHUNK_Z - 1) + CHUNK_X - 1);
	const int dz = stride_x - (CHUNK_X - 1);
	
	for(int i = 0; i<CHUNK_SIZE; )
	{
		Block cur = *data_ptr;

		intervals.insert(make_pair(i, cur));
		
		while(i<CHUNK_SIZE && *data_ptr == cur)
		{
			++i;
			if( i & (CHUNK_X - 1) )
				data_ptr += dx;
			else if( i & (CHUNK_X*CHUNK_Z - 1) )
				data_ptr += dz;
			else
				data_ptr += dy;
		}
	}
}

//Decompresses a chunk
void ChunkBuffer::decompress_chunk(Block* data, int stride_x, int stride_xz) const
{
	auto data_ptr = data;
	
	const int dx = 1;
	const int dy = stride_xz - (stride_x * (CHUNK_Z - 1) + CHUNK_X - 1);
	const int dz = stride_x - (CHUNK_X - 1);
	
	int i = 0;
	for(auto iter = intervals.begin(); iter != intervals.end(); )
	{
		auto block = iter->second;
		int right = (++iter == intervals.end()) ? CHUNK_SIZE : iter->first;
	
		assert(i < right);
	
		while(i < right)
		{
			*data_ptr = block;
		
			++i;
			if( i & (CHUNK_X - 1) )
				data_ptr += dx;
			else if( i & (CHUNK_X*CHUNK_Z - 1) )
				data_ptr += dz;
			else
				data_ptr += dy;
		}
	}
}

//Caches protocol buffer data
void ChunkBuffer::cache_protocol_buffer_data()
{
	pbuffer_data.clear();
	
	for(auto iter = intervals.begin(); iter != intervals.end(); )
	{
		auto left = iter->first;
		auto block = iter->second;
		int right = (++iter == intervals.end()) ? CHUNK_SIZE : iter->first;
		
		//Write out length bytes, encoded using varint encoding (similar to protobuf)
		//lower 7 bits store int part, highest bit means continue
		int len = right - left;
		assert(len > 0);
		
		while(len > 0x80)
		{
			pbuffer_data.push_back((len & 0x7f) | 0x80);
			len >>= 7;
		}
		pbuffer_data.push_back(len & 0x7f);
		
		//Write block type
		pbuffer_data.push_back(block.type());
		for(int i=0; i<block.state_bytes(); ++i)
		{
			pbuffer_data.push_back(block.state(i));
		}
	}
}


//Serializes a chunk buffer
bool ChunkBuffer::serialize_to_protocol_buffer(Network::Chunk& c) const
{
	if(pbuffer_data.size() == 0)
		return false;

	c.set_last_modified(timestamp);
	c.set_data(&pbuffer_data[0], pbuffer_data.size());
	return true;
}

//Parses a chunk from a protocol buffer, used for map serialization
void ChunkBuffer::parse_from_protocol_buffer(Network::Chunk const& c)
{
	if(!c.has_data())
		return;

	//Unpack fields from protocol buffer
	if(c.has_last_modified())
		timestamp = c.last_modified();
	pbuffer_data.assign(c.data().begin(), c.data().end());
	intervals.clear();

	//Unpack the ranges
	auto ptr = (uint8_t*)&pbuffer_data[0];
	for(int i=0; i<CHUNK_SIZE; )
	{
		//Unpack length
		int len = 0;
		for(int j=0; j<4; ++j)
		{
			uint8_t c = *(ptr++);
			len += (c & 0x7f) << (7 * j);
			if((c & 0x80) == 0)
				break;
		}
		
		assert(len > 0);
		
		//Unpack the encoded block type
		uint8_t type = *(ptr++);
		Block b(type, ptr);
		ptr += b.state_bytes();

		//Insert the interval and continue
		intervals.insert(make_pair(i, b));
		i += len;
	}
}

//Checks if the interval trees are equivalent
bool ChunkBuffer::equals(interval_tree_t const& tree) const
{
	auto a = intervals.begin();
	auto b = tree.begin();
	
	while( a != intervals.end() && b != tree.end() )
	{
		if( a->first  != b->first ||
			a->second != b->second )
		{
			return false;
		}
		++a;
		++b;
	}
	
	return
		a == intervals.end() &&
		b == tree.end();
}

};
