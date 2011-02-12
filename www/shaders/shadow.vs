attribute vec3 pos;

uniform mat4 proj;
uniform mat4 view;

varying float depth;

void main(void)
{
	vec4 pos = proj * view * vec4(pos, 1.0);
	
	depth = pos.z;
	gl_Position = pos;
}


