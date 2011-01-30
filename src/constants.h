#ifndef CONSTNATS_H
#define CONSTANTS_H

//Application wide constants go here

//Constants for different buffer sizes
#define PASSWORD_HASH_LEN		64
#define USER_NAME_MAX_LEN		20
#define USER_NAME_MIN_LEN		3

//Player name info
#define PLAYER_NAME_MAX_LEN		20

//Chunk dimensions
#define CHUNK_X_S				4
#define CHUNK_Y_S				4
#define CHUNK_Z_S				4

#define CHUNK_XY_S				(CHUNK_X_S + CHUNK_Y_S)

#define CHUNK_X					(1<<CHUNK_X_S)
#define CHUNK_Y					(1<<CHUNK_Y_S)
#define CHUNK_Z					(1<<CHUNK_Z_S)

#define CHUNK_X_MASK			(CHUNK_X-1)
#define CHUNK_Y_MASK			(CHUNK_Y-1)
#define CHUNK_Z_MASK			(CHUNK_Z-1)

//Index within a chunk
#define CHUNK_OFFSET(X,Y,Z)		((X)+((Y)<<CHUNK_X_S)+((Z)<<(CHUNK_X_S+CHUNK_Y_S)))

//Maximum length for a compressed chunk
#define MAX_CHUNK_BUFFER_LEN	(CHUNK_X*CHUNK_Y*CHUNK_Z*16)

//Number of bits for a chunk index
#define CHUNK_IDX_S				19ULL

//Size of max chunk index
#define CHUNK_IDX_MAX			(1<<CHUNK_IDX_S)

#define CHUNK_IDX_MASK			(CHUNK_IDX_MAX - 1)

//Coordinate indexing stuff
#define COORD_MIN_X				0
#define COORD_MIN_Y				0
#define COORD_MIN_Z				0

#define COORD_MAX_X				(CHUNK_IDX_MAX<<CHUNK_X_S)
#define COORD_MAX_Y				(CHUNK_IDX_MAX<<CHUNK_Y_S)
#define COORD_MAX_Z				(CHUNK_IDX_MAX<<CHUNK_Z_S)

#define COORD_BITS				(CHUNK_X_S + CHUNK_IDX_S)

//Map parameters
#define MAX_MAP_X	(1<<27)
#define MAX_MAP_Y	(1<<27)
#define MAX_MAP_Z	(1<<27)

//Converts a coordinate into a chunk index
#define COORD2CHUNKID(X,Y,Z)	(Game::ChunkID( ((X)>>(CHUNK_X_S)) & CHUNK_IDX_MASK, \
												((Y)>>(CHUNK_Y_S)) & CHUNK_IDX_MASK, \
												((Z)>>(CHUNK_Z_S)) & CHUNK_IDX_MASK ) )

//Precision for network coordinates
#define COORD_NET_PRECISION_S	6
#define COORD_NET_PRECISION		(1ULL<<COORD_NET_PRECISION_S)

//Radius for chat events
#define CHAT_RADIUS				128

//Radius of one network update region (for a player)
#define UPDATE_RADIUS			256

#define CHUNK_RADIUS			(2*UPDATE_RADIUS)

//Bounds on chat line
#define CHAT_LINE_MAX_LEN		128

//Radius beyond which the client needs to be resynchronized
#define POSITION_RESYNC_RADIUS	10

//In ticks, time before player updates are discarded
#define MAX_PING				200

//Radius around which one can dig
#define DIG_RADIUS				5

//Player start coordinates
#define PLAYER_START_X			(1 << 20)
#define PLAYER_START_Y			((1 << 20)+32)
#define PLAYER_START_Z			(1 << 20)

//Player time out (in ticks) default is approx. 2 minutes
#define PLAYER_TIMEOUT			3000

//Sleep time for the main loop
#define SLEEP_TIME				40

//Size of an integer query variable
#define INT_QUERY_LEN			32

//Length of the console command buffer
#define COMMAND_BUFFER_LEN		256

//Maximum size for an event packet
#define EVENT_PACKET_SIZE		(1<<16)

//Grid bucket sizes for range searching
#define BUCKET_SHIFT_X			7ULL
#define BUCKET_SHIFT_Y			7ULL
#define BUCKET_SHIFT_Z			7ULL

#define BUCKET_X				(1ULL<<BUCKET_SHIFT_X)
#define BUCKET_Y				(1ULL<<BUCKET_SHIFT_Y)
#define BUCKET_Z				(1ULL<<BUCKET_SHIFT_Z)

//Bucket string stuff
#define BUCKET_STR_BITS			6ULL
#define BUCKET_STR_MASK			((1ULL<<BUCKET_STR_BITS)-1ULL)
#define BUCKET_STR_LEN			5ULL

//Converts a triples into a bucket index
#define BUCKET_BITS_X			(32ULL - BUCKET_SHIFT_X)
#define BUCKET_BITS_Y			(32ULL - BUCKET_SHIFT_Y)
#define BUCKET_BITS_Z			(32ULL - BUCKET_SHIFT_Z)

#define BUCKET_MASK_X			((1ULL<<BUCKET_BITS_X) - 1)
#define BUCKET_MASK_Y			((1ULL<<BUCKET_BITS_Y) - 1)
#define BUCKET_MASK_Z			((1ULL<<BUCKET_BITS_Z) - 1)

#define TO_BUCKET(X,Y,Z) \
	((((uint64_t)(X)>>BUCKET_SHIFT_X) & BUCKET_MASK_X) | \
	((((uint64_t)(Y)>>BUCKET_SHIFT_Y) & BUCKET_MASK_Y)<<BUCKET_BITS_X) | \
	((((uint64_t)(Z)>>BUCKET_SHIFT_Z) & BUCKET_MASK_Z)<<(BUCKET_BITS_X+BUCKET_BITS_Y)))


#define BUCKET_IDX(X,Y,Z) \
	((uint64_t)(X) | \
	((uint64_t)(Y) << BUCKET_BITS_X) | \
	((uint64_t)(Z) << (BUCKET_BITS_X+BUCKET_BITS_Y)))

#endif


