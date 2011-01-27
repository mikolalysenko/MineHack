#ifdef GL_ES
precision highp float;
#endif

attribute vec4 pos;

uniform mat4 proj;
uniform mat4 view;

void main(void)
{
	gl_Position = proj * view * pos;
}


