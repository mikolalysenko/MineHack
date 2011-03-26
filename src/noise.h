#ifndef NOISE_H
#define NOISE_H

#include <stdint.h>

void setNoiseSeed(int64_t);

float simplexNoise1D(float xin, int64_t octaves);
float simplexNoise2D(float xin, float yin, int64_t octaves);
float simplexNoise3D(float xin, float yin, float zin, int64_t octaves);

int32_t pseudorand(int32_t a);
int64_t pseudorand_var(int64_t i, ...);

#endif
