#ifndef NOISE_H
#define NOISE_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void setNoiseSeed(int);
	
float simplexNoise2D(float xin, float yin, int octaves);
float simplexNoise3D(float xin, float yin, float zin, int octaves);
	
#ifdef __cplusplus
}
#endif // __cplusplus


#endif
