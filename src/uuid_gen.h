#ifndef RNG_H
#define RNG_H

#include <cstdlib>
#include <cstdio>
#include <cstdint>

extern "C" {
#include "dSFMT.h"
};

//A random number generator for creating id numbers
struct UUIDGenerator
{
	UUIDGenerator();
	
	uint64_t gen_uuid();
	
private:
	dsfmt_t	mt;
};


#endif

