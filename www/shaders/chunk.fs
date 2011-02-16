precision mediump float;

uniform sampler2D tex;
uniform float	shadow_fudge_factor;

varying vec4 tc;
varying vec3 sun_light_color;
varying vec3 light_color;
varying float depth;


uniform sampler2D shadow_tex0;
uniform sampler2D shadow_tex1;
uniform sampler2D shadow_tex2;

varying vec4 frag_pos0;
varying vec4 frag_pos1;
varying vec4 frag_pos2;

uniform float shadow_cutoff0;
uniform float shadow_cutoff1;


float linstep(float lo, float hi, float v)
{
	return clamp((v - lo) / (hi - lo), 0.0, 1.0);
}

float shadow_weight(float t, vec2 moments)
{
	float p = 0.0;
	if(t <= moments.x)
		p = 1.0;
		
	float variance = moments.y - moments.x * moments.x;
	float d = t - moments.x;
	float p_max = variance / (variance + d * d);
		
	return linstep(shadow_fudge_factor, 1.0, max(p, p_max));
}

vec2 get_moments(vec2 frag_pos, sampler2D shadow_tex)
{
	return texture2D(shadow_tex, frag_pos).xy;
}

float get_shadow()
{
	float t, s;
	vec2 moments;

	if(depth <= shadow_cutoff0 - 1.0)
	{
		t 		= frag_pos0.z;
		moments = get_moments(frag_pos0.xy, shadow_tex0);
	}
	else if(depth <= shadow_cutoff0)
	{
		s = fract(depth);
		t = mix(frag_pos0.z, frag_pos1.z, s);
		moments = mix(
			get_moments(frag_pos0.xy, shadow_tex0), 
			get_moments(frag_pos1.xy, shadow_tex1), s);
	}
	else if(depth <= shadow_cutoff1 - 1.0)
	{
		t 		= frag_pos1.z;
		moments = get_moments(frag_pos1.xy, shadow_tex1);
	}
	else if(depth <= shadow_cutoff1)
	{
		s = fract(depth);
		t = mix(frag_pos1.z, frag_pos2.z, s);
		moments = mix(
			get_moments(frag_pos1.xy, shadow_tex1), 
			get_moments(frag_pos2.xy, shadow_tex2), s);
	}
	else
	{
		t 		= frag_pos2.z;
		moments = get_moments(frag_pos2.xy, shadow_tex2);
	}
	
	return shadow_weight(t, moments);
}


void main(void)
{
	vec4 texColor = texture2D(tex, (tc.zw + fract(tc.xy)) / 16.0 );
	
	vec3 sunColor = get_shadow() * sun_light_color;
	
	vec3 rgbColor = clamp(texColor.xyz * (light_color * 0.1 + sunColor) * texColor.w, 0.0, 1.0);
	
	/*
	if(depth <= shadow_cutoff0)
		rgbColor.r += 0.8;
	else if(depth <= shadow_cutoff1)
		rgbColor.g += 0.8;
	else
		rgbColor.b += 0.8;
	*/
	
	gl_FragColor = vec4(rgbColor, texColor.w);
}

