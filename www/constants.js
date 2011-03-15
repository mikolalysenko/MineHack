//Rates for the different game intervals
const GAME_TICK_RATE		= 20;
const GAME_DRAW_RATE		= 40;
const GAME_SHADOW_RATE		= 80;
const GAME_NET_UPDATE_RATE	= 200;


const DOMAIN_NAME = "127.0.0.1:8081"

//Chunk parameters
const CHUNK_X_S		= 4;
const CHUNK_Y_S		= 4;
const CHUNK_Z_S		= 4;

const CHUNK_XY_S	= CHUNK_X_S + CHUNK_Y_S;

const CHUNK_X_STEP	= 1;
const CHUNK_Y_STEP	= (1 << CHUNK_X_S);
const CHUNK_Z_STEP	= (1 << CHUNK_XY_S);

const CHUNK_X		= (1<<CHUNK_X_S);
const CHUNK_Y		= (1<<CHUNK_Y_S);
const CHUNK_Z		= (1<<CHUNK_Z_S);

const CHUNK_X_MASK	= CHUNK_X - 1;
const CHUNK_Y_MASK	= CHUNK_Y - 1;
const CHUNK_Z_MASK	= CHUNK_Z - 1;

const CHUNK_SIZE	= CHUNK_X * CHUNK_Y * CHUNK_Z;

const CHUNK_DIMS	= [ CHUNK_X, CHUNK_Y, CHUNK_Z ];

//Event types
const EV_START				= 0;
const EV_SET_BLOCK			= 1;
const EV_FETCH_CHUNK		= 2;
const EV_VB_UPDATE			= 3;
const EV_CHUNK_UPDATE		= 4;
const EV_PRINT				= 5;
const EV_FORGET_CHUNK		= 6;
const EV_SET_THROTTLE		= 7;
const EV_VB_COMPLETE		= 8;
const EV_RECV				= 9;
const EV_CRASH				= 10;
const EV_SEND				= 11;
