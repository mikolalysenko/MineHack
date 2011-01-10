#include "misc.h"
#include "world.h"

namespace Game
{

//The world instance, handles game logic stuff
World::World()
{
	running = true;
	gen = new WorldGen();
	game_map = new Map(gen);
}

//Adds an event to the world
void World::add_event(const InputEvent& ev)
{

}

//Retrieves a compressed chunk from the server
int World::get_compressed_chunk(
	const Server::SessionID& session_id, 
	const ChunkId& chunk_id,
	void* buf,
	size_t buf_len)
{
	//Lock region for reading
	RegionReadLock lock(game_map, chunk_id);

	Chunk* chunk = game_map->get_chunk(chunk_id);
	
	return chunk->compress(buf, buf_len);
}
	
//Sends queued messages to client
int World::heartbeat(
	const Server::SessionID&,
	void* buf,
	size_t buf_len)
{
	return 0;
}
	
//Ticks the server
void World::tick()
{	
}

};
