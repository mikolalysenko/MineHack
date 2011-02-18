precision mediump float;

varying float depth;

void main(void)
{
	gl_FragColor = vec4(depth, depth*depth, 0, 1);
}

