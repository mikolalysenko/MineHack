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

#define CHUNK_X					(1<<CHUNK_X_S)
#define CHUNK_Y					(1<<CHUNK_Y_S)
#define CHUNK_Z					(1<<CHUNK_Z_S)

#define CHUNK_X_MASK			(CHUNK_X-1)
#define CHUNK_Y_MASK			(CHUNK_Y-1)
#define CHUNK_Z_MASK			(CHUNK_Z-1)

//Index within a chunk
#define CHUNK_OFFSET(X,Y,Z)		((X)+((Y)<<CHUNK_X_S)+((Z)<<(CHUNK_X_S+CHUNK_Y_S)))

//Maximum length for a compressed chunk
#define MAX_CHUNK_BUFFER_LEN	(CHUNK_X*CHUNK_Y*CHUNK_Z*2)

//Number of bits for a chunk index
#define CHUNK_IDX_S				21ULL

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

//Converts a coordinate into a chunk index
#define COORD2CHUNKID(X,Y,Z)	(Game::ChunkID( ((X)>>(CHUNK_X_S)) & CHUNK_IDX_MASK, \
												((Y)>>(CHUNK_Y_S)) & CHUNK_IDX_MASK, \
												((Z)>>(CHUNK_Z_S)) & CHUNK_IDX_MASK ) )


//Precision for network coordinates
#define COORD_NET_PRECISION_S	(32ULL - COORD_BITS)
#define COORD_NET_PRECISION		(1ULL<<COORD_NET_PRECISION_S)

//Radius for chat events
#define CHAT_RADIUS				128

//Bounds on chat line
#define CHAT_LINE_MAX_LEN		128

//Radius beyond which the client needs to be resynchronized
#define POSITION_RESYNC_RADIUS	8

//Size of the update region for a heartbeat
#define UPDATE_RADIUS			128


#define TICK_RESYNC_TIME		100

#define PLAYER_START_X			(1 << 20)
#define PLAYER_START_Y			(1 << 20)
#define PLAYER_START_Z			(1 << 20)

//Sleep time for the main loop
#define SLEEP_TIME				40

//Size of an integer query variable
#define INT_QUERY_LEN			32

//Length of the console command buffer
#define COMMAND_BUFFER_LEN		256

//Maximum size for an event packet
#define EVENT_PACKET_SIZE		(1<<16)

//Grid bucket sizes for range searching
#define BUCKET_SHIFT_X			6
#define BUCKET_SHIFT_Y			6
#define BUCKET_SHIFT_Z			6

#define BUCKET_MASK_X			((1<<BUCKET_SHIFT_X)-1)
#define BUCKET_MASK_Y			((1<<BUCKET_SHIFT_Y)-1)
#define BUCKET_MASK_Z			((1<<BUCKET_SHIFT_Z)-1)

#define BUCKET_X				(1<<BUCKET_SHIFT_X)
#define BUCKET_Y				(1<<BUCKET_SHIFT_Y)
#define BUCKET_Z				(1<<BUCKET_SHIFT_Z)

#define BUCKET_STR_LEN			5


#endif


