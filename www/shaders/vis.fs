#ifdef GL_ES
precision highp float;
#endif


uniform vec4 chunk_id;

void main(void)
{
	gl_FragColor = chunk_id;
}

