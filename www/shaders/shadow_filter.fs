precision mediump float;
uniform sampler2D tex;
varying vec2 tc;

void main(void)
{
	vec4 result = vec4(0.0,0.0,0.0,0.0);

	for(int i=-1; i<=1; ++i)
	for(int j=-1; j<=1; ++j)
	{
		result += texture2D(tex, tc + vec2(2.0*float(i)+0.5, 2.0*float(j)+0.5)/256.0);
	}

	gl_FragColor = result / 9.0;
}

