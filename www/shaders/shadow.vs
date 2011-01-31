#ifdef GL_ES
precision highp float;
#endif

attribute vec4 pos;

uniform mat4 proj;
uniform mat4 view;

varying vec4 depth;

void main(void)
{
	vec4 pos = proj * view * pos;
	
	depth = vec4(pos.w, pos.w*pos.w, 0, 0);
	gl_Position = pos;
}


