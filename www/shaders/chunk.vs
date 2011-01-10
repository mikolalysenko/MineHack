#ifdef GL_ES
precision highp float;
#endif

attribute vec3 pos;
attribute vec2 texCoord;

uniform mat4 proj;
uniform mat4 view;

varying vec4 tc;

void main(void)
{
	gl_Position = proj * view * vec4(pos.x, pos.y, pos.z, 1.0);
	
	vec2 t = texCoord * 16.0;
	
	tc = vec4(fract(t), floor(t));
}

