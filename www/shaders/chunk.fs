#ifdef GL_ES
precision highp float;
#endif

uniform sampler2D tex;

varying vec4 tc;

void main(void)
{
	gl_FragColor = texture2D(tex, (tc.zw + fract(tc.xy)) / 16.0 );
}

