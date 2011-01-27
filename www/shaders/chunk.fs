#ifdef GL_ES
precision highp float;
#endif

uniform sampler2D tex;

varying vec4 tc;
varying float radiosity;

void main(void)
{
	vec4 color = clamp(texture2D(tex, (tc.zw + fract(tc.xy)) / 16.0 ) * radiosity, 0.0, 1.0);

	gl_FragColor = vec4(color.xyz, 1);
}

