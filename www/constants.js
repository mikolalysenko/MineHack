/*jslint strict: true, undef: true, onevar: true, evil: true, es5: true, adsafe: false, regexp: true, maxerr: 50, indent: 4 */


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
const NET_COORD_PRECISION 	= 256;
const NET_DIVIDE			= 1.0 / NET_COORD_PRECISION;

//Network header size
const NET_HEADER_SIZE		= 30;


//Entity type codes
const PLAYER_ENTITY		= 1;
const MONSTER_ENTITY	= 2;


//A fixed delay on the ping, always added
const PING_DELAY		= 5;

//Action event codes
const ACTION_DIG_START		= 0;
const ACTION_DIG_STOP		= 1;
const ACTION_USE_ITEM		= 2;

//Action target codes
const TARGET_NONE			= 0;
const TARGET_BLOCK			= 1;
const TARGET_ENTITY			= 2;
const TARGET_RAY			= 3;
const TARGET_HAS_ITEM		= (1<<7);

const DIG_RADIUS			= 5;

