#ifndef PHYSICS_H
#define PHYSICS_H

struct Physics
{
	void mark_chunk(ChunkID const& chunk);	
	void update();
	
private:

	void update_region(std::vector<ChunkID> chunks);

};

#endif

