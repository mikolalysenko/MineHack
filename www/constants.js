
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

//Network precision
const NET_COORD_PRECISION = 128;


//Entity type codes
const PLAYER_ENTITY		= 1;
const MONSTER_ENTITY	= 2;


