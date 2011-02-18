precision mediump float;

attribute vec3 pos;
attribute vec2 texCoord;
attribute vec3 normal;
attribute vec3 lightColor;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;

//Shadow map parameters
uniform mat4 shadow;

//Sunlight parameters
uniform vec3 sun_dir;
uniform vec3 sun_color;

varying vec4 tc;
varying vec3 light_color;
varying vec3 sun_light_color;
varying float depth;
varying vec3 shadow_pos;

void main(void)
{
	vec4 tpos = view * vec4(pos, 1);


	//Compute position
	gl_Position = proj * tpos;
	
	//Depth
	depth = gl_Position.z / gl_Position.w;
	
	//Shadow map coordinates
	shadow_pos = (shadow * tpos).xyz;
	
	//Texture coordinates
	vec2 t = texCoord.xy * 16.0;
	tc = vec4(fract(t), floor(t));

	//Lighting stuff	
	sun_light_color = clamp(dot(sun_dir, normal), 0.0, 1.0) * sun_color;
	light_color = lightColor;
}

