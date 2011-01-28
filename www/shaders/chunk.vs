#ifdef GL_ES
precision highp float;
#endif

attribute vec4 pos;
attribute vec4 texCoord;

uniform mat4 proj;
uniform mat4 view;

varying vec4 tc;
varying float radiosity;

void main(void)
{
	gl_Position = proj * view * pos;
	
	vec2 t = texCoord.xy * 16.0;
	tc = vec4(fract(t), floor(t));
	
	radiosity = texCoord.z *.5 + .5;
}

