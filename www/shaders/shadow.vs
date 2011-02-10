attribute vec4 pos;

uniform mat4 proj;
uniform mat4 view;

varying float depth;

void main(void)
{
	vec4 pos = proj * view * pos;
	
	depth = pos.z;
	gl_Position = pos;
}


