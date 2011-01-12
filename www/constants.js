
//Chunk parameters
const CHUNK_X_S		= 5;
const CHUNK_Y_S		= 5;
const CHUNK_Z_S		= 5;

const CHUNK_XY_S	= CHUNK_X_S + CHUNK_Y_S;

const CHUNK_X_STEP	= 1;
const CHUNK_Y_STEP	= (1 << CHUNK_X_S);
const CHUNK_Z_STEP	= (1 << CHUNK_XY_S);

const CHUNK_X		= (1<<CHUNK_X_S);
const CHUNK_Y		= (1<<CHUNK_Y_S);
const CHUNK_Z		= (1<<CHUNK_Z_S);

const CHUNK_X_MASK	= 1 - CHUNK_X;
const CHUNK_Y_MASK	= 1 - CHUNK_Y;
const CHUNK_Z_MASK	= 1 - CHUNK_Z;


