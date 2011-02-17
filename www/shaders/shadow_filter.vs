attribute vec4 pos;

varying vec2 tc;

void main(void)
{
	tc = 0.5 * pos.xy + vec2(0.5, 0.5);
	gl_Position = pos;
}


