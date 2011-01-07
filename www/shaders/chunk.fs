#ifdef GL_ES
precision highp float;
#endif

varying float ao;

void main(void)
{
	gl_FragColor = vec4(ao, ao, ao, ao);
}

