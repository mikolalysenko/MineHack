#ifndef GAME_MAP_H
#define GAME_MAP_H

#include <stdint.h>

#include <tbb/atomic.h>
#include <tbb/compat/thread>
#include <tbb/spin_rw_mutex.h>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/concurrent_hash_map.h>

#include <tcutil.h>
#include <tchdb.h>

#include "constants.h"
#include "config.h"
#include "chunk.h"
#include "worldgen.h"

namespace Game
{
	
	//This is basically a data structure which implements a caching/indexing system for chunks
	//The goal is to keep the entire database in memory at all times for maximum performance.
	//In order to acheive this, it is necessary to operate directly on compressed chunks.
	struct GameMap
	{
		//Types
		typedef tbb::concurrent_hash_map<ChunkID, ChunkBuffer*, ChunkIDHashCompare> chunk_map_t;
		typedef chunk_map_t::accessor accessor;
		typedef chunk_map_t::const_accessor const_accessor;

		//Map constructor
		GameMap(Config* config);
		~GameMap();
		
		//Accessor methods
		void get_chunk_buffer(accessor&, ChunkID const&);
		void get_chunk_buffer(const_accessor&, ChunkID const&);
		void get_surface_chunk_buffer(accessor&, ChunkID const&);
		void get_surface_chunk_buffer(const_accessor&, ChunkID const&);
		
		//Block accessor methods
		Block get_block(int x, int y, int z);
		
		//Chunk update methods
		void get_chunk(
			ChunkID const&, 
			Block* buffer, 
			int stride_x = CHUNK_X, 
			int stride_xz = CHUNK_X * CHUNK_Z);
		
		bool update_chunk(
			ChunkID const&,
			uint64_t t,
			Block* buffer,
			int stride_x = CHUNK_X,
			int stride_xz = CHUNK_X * CHUNK_Z);
		
		//Retrieves a chunk's protocol buffer
		Network::Chunk* get_chunk_pbuffer(ChunkID const&);

		//Protocol buffer methods
		Network::ServerPacket* get_net_chunk(ChunkID const&, uint64_t timestamp);
		
		//Saves the state of the map
		void serialize();
					
	private:
		//The world generator and config stuff
		WorldGen* world_gen;
		Config* config;
		
		//Database/persistence stuff
		typedef tbb::concurrent_unordered_map<ChunkID, bool, ChunkIDHashCompare> write_set_t;
		TCHDB*	map_db;
		tbb::spin_rw_mutex	write_set_lock;
		write_set_t pending_writes;
		std::thread* db_worker_thread;
		tbb::atomic<bool> running;
		
		void initialize_db();
		void shutdown_db();
		void mark_dirty(ChunkID const&);
		
		//The game map
		// When operating on surface chunks and chunk remember the locking order:
		//	1.  Always lock surface_chunks before chunks
		//	2.	Lock chunks in order of chunk id; ie lexicographic y, z, x, low-to-high.
		//
		chunk_map_t chunks, surface_chunks;
		
		//Chunk generation stuff
		void generate_chunk(accessor&, ChunkID const&);
		void generate_surface_chunk(accessor&, ChunkID const&);
	};
	
};


#endif

