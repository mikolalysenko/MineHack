attribute vec3 pos;

uniform mat4 proj;
uniform mat4 view;

void main(void)
{
	mat4 k = proj * view;

	gl_Position = k * 0.1 * vec4(pos, 1.0);
}

