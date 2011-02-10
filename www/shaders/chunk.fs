precision mediump float;
uniform sampler2D tex;

varying vec4 tc;
varying vec3 light_color;

void main(void)
{
	vec4 texColor = texture2D(tex, (tc.zw + fract(tc.xy)) / 16.0 );
	vec3 rgbColor = clamp(texColor.xyz * light_color * texColor.w, 0.0, 1.0);

	gl_FragColor = vec4(rgbColor.xyz, texColor.w);
}

