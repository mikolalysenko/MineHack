precision mediump float;

uniform sampler2D tex;
uniform sampler2D shadow_tex;
uniform float	shadow_fudge_factor;

varying vec4 tc;
varying vec4 frag_pos;
varying vec3 sun_light_color;
varying vec3 light_color;


float linstep(float lo, float hi, float v)
{
	return clamp((v - lo) / (hi - lo), 0.0, 1.0);
}

float shadow_weight()
{
	float t = frag_pos.z - 0.0005;
	vec2 moments = texture2D(shadow_tex, frag_pos.xy).xy;
	
	float p = 0.0;
	if(t <= moments.x)
		p = 1.0;
		
	float variance = moments.y - moments.x * moments.x;
	float d = t - moments.x;
	float p_max = variance / (variance + d * d);
		
	return linstep(shadow_fudge_factor, 1.0, max(p, p_max));
}



void main(void)
{
	vec4 texColor = texture2D(tex, (tc.zw + fract(tc.xy)) / 16.0 );
	
	vec3 sunColor = shadow_weight() * sun_light_color;
	
	vec3 rgbColor = clamp(texColor.xyz * (light_color * 0.1 + sunColor) * texColor.w, 0.0, 1.0);

	gl_FragColor = vec4(rgbColor, texColor.w);
}

