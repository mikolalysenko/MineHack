#ifdef GL_ES
precision highp float;
#endif

uniform sampler2D tex;

varying vec4 tc;
varying float radiosity;

void main(void)
{
/*
	vec4 texColor = texture2D(tex, (tc.zw + fract(tc.xy)) / 16.0 );

	vec3 rgbColor = clamp(texColor.xyz * radiosity, 0.0, 1.0);

	gl_FragColor = vec4(rgbColor.xyz, texColor.w);
*/
	vec4 texColor = texture2D(tex, (tc.zw + fract(tc.xy)) / 16.0 );


	gl_FragColor = vec4(radiosity, radiosity, radiosity, texColor.x * 1000.0);
}

