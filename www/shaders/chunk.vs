precision mediump float;

attribute vec3 pos;
attribute vec2 texCoord;
attribute vec3 normal;
attribute vec3 lightColor;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 shadow;

//Sunlight shadow map parameters

//Sunlight parameters
uniform vec3 sun_dir;
uniform vec3 sun_color;

varying vec4 tc;
varying vec3 light_color;
varying vec3 sun_light_color;
varying vec4 frag_pos;

void main(void)
{
	vec4 tpos = view * vec4(pos, 1);

	gl_Position = proj * tpos;
	frag_pos = shadow * tpos;
	
	vec2 t = texCoord.xy * 16.0;
	tc = vec4(fract(t), floor(t));
	
	sun_light_color = clamp(dot(sun_dir, normal), 0.0, 1.0) * sun_color;
	light_color = lightColor;
}

