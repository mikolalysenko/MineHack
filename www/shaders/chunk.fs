precision mediump float;

uniform sampler2D tex;
uniform float	shadow_fudge_factor;

varying vec4 tc;
varying vec3 sun_light_color;
varying vec3 light_color;
varying float depth;


uniform sampler2D shadow_tex;
varying vec3 shadow_pos;


float linstep(float lo, float hi, float v)
{
	return clamp((v - lo) / (hi - lo), 0.0, 1.0);
}

float shadow_weight(float t, vec2 moments)
{
	t -= 0.001;

	float p = 0.0;
	if(t <= moments.x)
		p = 1.0;
		
	float variance = moments.y - moments.x * moments.x;
	float d = t - moments.x;
	float p_max = variance / (variance + d * d);
		
	return linstep(shadow_fudge_factor, 1.0, max(p, p_max));
}

float get_shadow()
{
	return shadow_weight(shadow_pos.z, texture2D(shadow_tex, shadow_pos.xy).xy);
}

void main(void)
{
	vec4 texColor = texture2D(tex, (tc.zw + fract(tc.xy)) / 16.0 );
	
	vec3 sunColor = get_shadow() * sun_light_color;
	
	vec3 rgbColor = clamp(texColor.xyz * (light_color * 0.2 + sunColor) * texColor.w, 0.0, 1.0);
	
	gl_FragColor = vec4(rgbColor, texColor.w);
}

