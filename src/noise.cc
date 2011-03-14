#include "noise.h"

#include <time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

static int64_t grad3[12][3] = {
{1,1,0},
{-1,1,0},
{1,-1,0},
{-1,-1,0},
{1,0,1},
{-1,0,1},
{1,0,-1},
{-1,0,-1},
{0,1,1},
{0,-1,1},
{0,1,-1},
{0,-1,-1}};

int64_t seed = 0;
void setNoiseSeed(int64_t i)
{
	seed = i;
}

int32_t pseudorand(int32_t a)
{
	a += seed;
    a = (a ^ 61) ^ (a >> 16);
    a = a + (a << 3);
    a = a ^ (a >> 4);
    a = a * 0x27d4eb2d;
    a = a ^ (a >> 15);
    return a;
}

int64_t pseudorand_var(int64_t i, ...)
{
	va_list args;
	va_start(args, i);
	
	int64_t retval = 0;
	for(int64_t x = 0; x < i; x++)
		retval = pseudorand(retval + va_arg(args, int64_t));
	
	va_end(args);
	
	return retval;
}

//calculate the floor of a float
int64_t fastfloor(float x)
{
	return x>0 ? (int64_t)x : (int64_t)x-1;
}

float dot2D(int64_t g[], float x, float y)
{
	return g[0]*x + g[1]*y;
}

float dot3D(int64_t g[], float x, float y, float z)
{
	return g[0]*x + g[1]*y + g[2]*z;
}

float contrib1D(int64_t xco, int64_t yco, float x, float y)
{
	int64_t gi = (pseudorand(xco) % 3) - 1;
	float t = 0.5 - x*x;
	
	if(t < 0)
		return 0.0;
	
	t *= t;
	return t * t * gi * x;
}

float contrib2D(int64_t xco, int64_t yco, float x, float y)
{
	int64_t gi = pseudorand_var(2, xco, yco) % 12;
	float t = 0.5 - x*x - y*y;
	
	if(t < 0)
		return 0.0;
	
	t *= t;
	return t * t * dot2D(grad3[gi], x, y);
}

float contrib3D(int64_t xco, int64_t yco, int64_t zco, float x, float y, float z)
{
	int64_t gi = pseudorand_var(3, xco, yco, zco) % 12;
	
	float t = 0.6 - x*x - y*y - z*z;
	
	if(t < 0)
		return 0.0;
	
	t *= t;
	return t * t * dot3D(grad3[gi], x, y, z);
}

// 1D simplex noise
float simplexNoise1D(float xin, float yin, int64_t octaves)
{
	/*//base case
	if(octaves <= 0)
		return 0;

	const float F1 = 0.414213562;
	const float G1 = 0.292893219;
	
	// Skew the input space to determine which simplex cell we're in
	float s = (xin)*F1;
	int64_t i = fastfloor(xin+s);
	
	float t = i*G1;
	float X0 = i-t; // Unskew the cell origin back to (x,y) space
	float x0 = xin-X0; // The x,y distances from the cell origin
	
	// Work out the hashed gradient indices of the three simplex corners
	int64_t ii = i & 255;
	
	// Calculate the contribution from the three corners
	float retval = 0;
	retval += contrib1D(ii, x0);
	retval += contrib1D(ii + 1, x2);
	
	//scale the output by 35 and add .5 so that it returns values in the int64_terval [0, 1]
	retval *= 35.0;
	retval += .5;
	
	// Add contributions from each corner to get the final noise value.
	// The result is scaled to return values in the interval [-1,1].
	return retval + (.5 * simplexNoise2D(xin * 2, yin * 2, octaves - 1));*/
	return 0;
}

// 2D simplex noise
float simplexNoise2D(float xin, float yin, int64_t octaves)
{
	//base case
	if(octaves <= 0)
		return 0;
	
	const float F2 = 0.366025404;
	const float G2 = 0.211324865;
	
	// Skew the input space to determine which simplex cell we're in
	float s = (xin+yin)*F2; // Hairy factor for 2D
	int64_t i = fastfloor(xin+s);
	int64_t j = fastfloor(yin+s);
	
	float t = (i+j)*G2;
	float X0 = i-t; // Unskew the cell origin back to (x,y) space
	float Y0 = j-t;
	float x0 = xin-X0; // The x,y distances from the cell origin
	float y0 = yin-Y0;
	
	// For the 2D case, the simplex shape is an equilateral triangle.
	// Determine which simplex we are in.
	int64_t i1, j1; // Offsets for second (middle) corner of simplex in (i,j) coords
	if(x0>y0)
	{
		i1=1;
		j1=0;
	} // lower triangle, XY order: (0,0)->(1,0)->(1,1)
	else
	{
		i1=0;
		j1=1;
	}
	
	// upper triangle, YX order: (0,0)->(0,1)->(1,1)
	// A step of (1,0) in (i,j) means a step of (1-c,-c) in (x,y), and
	// a step of (0,1) in (i,j) means a step of (-c,1-c) in (x,y), where
	// c = (3-sqrt(3))/6
	float x1 =x0-i1 + G2; // Offsets for middle corner in (x,y) unskewed coords
	float y1 =y0-j1 + G2;
	float x2=x0-1.0 + 2.0 * G2; // Offsets for last corner in (x,y) unskewed coords
	float y2=y0-1.0 + 2.0 * G2;

	// Work out the hashed gradient indices of the three simplex corners
	int64_t ii = i & 255;
	int64_t jj = j & 255;
	
	// Calculate the contribution from the three corners
	float retval = 0;
	retval += contrib2D(ii, jj, x0, y0);
	retval += contrib2D(ii + i1, jj + j1, x1, y1);
	retval += contrib2D(ii + 1, jj + 1, x2, y2);
	
	//scale the output by 17.5 and add .25 so that it returns values in the interval [0, .5]
	retval *= 17.5;
	retval += .25;
	
	// Add contributions from each corner to get the final noise value.
	// The result is scaled to return values in the interval [-1,1].
	return retval + (.5 * simplexNoise2D(xin * 2, yin * 2, octaves - 1));
}

float simplexNoise3D(float xin, float yin, float zin, int64_t octaves)
{
	if(octaves <= 0)
		return 0;
	
	// Skew the input space to determine which simplex cell we're in
	const float F3 = 1.0/3.0;
	const float G3 = 1.0/6.0; // Very nice and simple unskew factor, too
	
	float s = (xin+yin+zin)*F3; // Very nice and simple skew factor for 3D
	int64_t i = fastfloor(xin+s);
	int64_t j = fastfloor(yin+s);
	int64_t k = fastfloor(zin+s);
	float t = (i+j+k)*G3;
	float X0 = i-t; // Unskew the cell origin back to (x,y,z) space
	float Y0 = j-t;
	float Z0 = k-t;
	float x0 = xin-X0; // The x,y,z distances from the cell origin
	float y0 = yin-Y0;
	float z0 = zin-Z0;
	
	// For the 3D case, the simplex shape is a slightly irregular tetrahedron.
	// Determine which simplex we are in.
	int64_t i1, j1, k1; // Offsets for second corner of simplex in (i,j,k) coords
	int64_t i2, j2, k2; // Offsets for third corner of simplex in (i,j,k) coords
	if(x0>=y0)
	{
		if(y0>=z0){i1=1; j1=0; k1=0; i2=1; j2=1; k2=0;} // X Y Z order
		else if(x0>=z0){i1=1; j1=0; k1=0; i2=1; j2=0; k2=1;} // X Z Y order
		else{i1=0; j1=0; k1=1; i2=1; j2=0; k2=1;} // Z X Y order
	}
	else
	{ // x0<y0
		if(y0<z0) { i1=0; j1=0; k1=1; i2=0; j2=1; k2=1; } // Z Y X order
		else if(x0<z0) { i1=0; j1=1; k1=0; i2=0; j2=1; k2=1; } // Y Z X order
		else { i1=0; j1=1; k1=0; i2=1; j2=1; k2=0; } // Y X Z order
	}
	
	//A step of (1,0,0) in (i,j,k) means a step of (1-c,-c,-c) in (x,y,z),
	//a step of (0,1,0) in (i,j,k) means a step of (-c,1-c,-c) in (x,y,z), and
	//a step of (0,0,1) in (i,j,k) means a step of (-c,-c,1-c) in (x,y,z), where
	//c = 1/6.
	float x1 = x0 - i1 + G3; // Offsets for second corner in (x,y,z) coords
	float y1 = y0 - j1 + G3;
	float z1 = z0 - k1 + G3;
	float x2 = x0 - i2 + 2.0*G3; // Offsets for third corner in (x,y,z) coords
	float y2 = y0 - j2 + 2.0*G3;
	float z2 = z0 - k2 + 2.0*G3;
	float x3 = x0 - 1.0 + 3.0*G3; // Offsets for last corner in (x,y,z) coords
	float y3 = y0 - 1.0 + 3.0*G3;
	float z3 = z0 - 1.0 + 3.0*G3;

	// Work out the hashed gradient indices of the four simplex corners
	int64_t ii = i & 255;
	int64_t jj = j & 255;
	int64_t kk = k & 255;

	float retval = 0;
	retval += contrib3D(ii   , jj   , kk   , x0, y0, z0);
	retval += contrib3D(ii+i1, jj+j1, kk+k1, x1, y1, z1);
	retval += contrib3D(ii+i2, jj+j2, kk+k2, x2, y2, z2);
	retval += contrib3D(ii+1 , jj+1 , kk+1 , x3, y3, z3);
	
	retval *= 8.0;
	retval += .25;
	
	return retval + (.5 * simplexNoise3D(xin * 2, yin * 2, zin * 2, octaves - 1));
}
