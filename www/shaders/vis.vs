precision mediump float;

attribute vec3 pos;

uniform mat4 proj;
uniform vec3 chunk_offset;

void main(void)
{
	gl_Position = proj * vec4(pos + chunk_offset, 1);
}


