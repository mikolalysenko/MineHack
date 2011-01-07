#ifdef GL_ES
precision highp float;
#endif

attribute vec3 pos;

uniform mat4 proj;
uniform vec3 cube_loc;

void main(void)
{
	gl_Position = proj * view * vec4(pos.x, pos.y, pos.z, 1.0);
}

