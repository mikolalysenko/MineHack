#ifndef MAP_H
#define MAP_H

#include <string>
#include <map>

#include <cstdint>
#include <cstdlib>

#include <pthread.h>

#include <tcutil.h>
#include <tchdb.h>

#include "constants.h"
#include "chunk.h"
#include "worldgen.h"

#include "network.pb.h"

namespace Game
{

	//A visibility buffer index
	struct VisID
	{
		uint32_t x, y, z;
		
		VisID();
		VisID(uint64_t hash);
		VisID(Network::Coordinate const&);
		VisID(Network::ChunkID const&);
		VisID(VisID const&)
		VisID(Coord const&);
		VisID(ChunkID const&);

		VisID& operator=(const VisID& other)
		{
			x = other.x;
			y = other.y;
			z = other.z;
			return *this;
		}
		
		uint64_t hash() const
		{
			return (uint64_t)x + ((uint64_t)y<<20ULL) + ((uint64_t)z<<40ULL);
		}
	};


	//This is basically a data structure which implements a caching/indexing system for chunks
	struct Map
	{
		//Map constructor
		Map(WorldGen *w, std::string const& filename);
		~Map();
		
		//Retrieves the specific chunk
		void get_chunk(ChunkID const&, Chunk* res);
		
		//Returns a chunk with only surface cells
		//Interior cells are converted to special unknown type to prevent players from cheating
		bool get_surface_chunk(ChunkID const&, Network::Chunk& res);
		
		//Sets a block
		void set_block(int x, int y, int z, Block t);
		
		//Gets a block from a chunk
		Block get_block(int x, int y, int z);
		
		//Adds a set of visible chunks to the given network packet
		void add_vis_chunks_to_packet(VisID const& vis_id, Network::ChunkPacket& packet);
		
		//Generates a visibility buffer
		void gen_vis_buffer(VisID const& vis_id, bool no_lock = false);

		//Invalidates a visibility buffer
		void invalidate_vis_buffer(VisID vis_id);
		
	private:
		TCHDB* map_db;
		
		//A set of precalculated visibility buffers (these are regenerated constantly)
		pthread_mutex_t	vis_lock;
		std::map< uint64_t, std::vector< Network::Chunk* > >	vis_buffers;
		
		//The world generator
		WorldGen* world_gen;
	};
	
};


#endif

