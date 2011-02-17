precision mediump float;

varying float depth;

void main(void)
{

	#ifdef GL_OES_standard_derivatives
		float ddx = dFdx(depth);
		float ddy = dFdy(depth);
	
		gl_FragColor = vec4(depth, depth*depth + 0.25 * (ddx*ddx + ddy*ddy), 0, 1);
	#else
		gl_FragColor = vec4(depth, depth*depth, 0, 1);
	#endif
}

