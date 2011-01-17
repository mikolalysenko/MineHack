#include <cstdlib>
#include <cstdio>
#include <cstdint>

extern "C" {
#include "dSFMT.h"
};

#include "uuid_gen.h"

using namespace std;

UUIDGenerator::UUIDGenerator()
{
	int seed;
	FILE* urand = fopen("/dev/urandom", "rb");
	fread(&seed, sizeof(seed), 1, urand);
	fclose(urand);
	
	dsfmt_init_gen_rand(&mt, seed);
}

uint64_t UUIDGenerator::gen_uuid()
{
	int lo = dsfmt_genrand_uint32(&mt),
		hi = dsfmt_genrand_uint32(&mt);
	return (uint64_t)lo | (((uint64_t)hi)<<32ULL);
}

