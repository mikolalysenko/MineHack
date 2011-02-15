precision mediump float;

attribute vec3 pos;
attribute vec2 texCoord;
attribute vec3 normal;
attribute vec3 lightColor;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;

//Shadow map parameters
uniform mat4 shadow0;
uniform mat4 shadow1;
uniform mat4 shadow2;


//Sunlight parameters
uniform vec3 sun_dir;
uniform vec3 sun_color;

varying vec4 tc;
varying vec3 light_color;
varying vec3 sun_light_color;
varying float depth;

varying vec4 frag_pos0;
varying vec4 frag_pos1;
varying vec4 frag_pos2;

void main(void)
{
	vec4 tpos = view * vec4(pos, 1);

	vec4 spos = model * tpos;
	
	depth = -spos.z / spos.w;   

	//Compute position
	gl_Position = proj * tpos;
	
	//Compute shadow mapstuff
	frag_pos0 = shadow0 * tpos - vec4(0.0, 0.0, 0.001, 0.0);
	frag_pos1 = shadow1 * tpos - vec4(0.0, 0.0, 0.001, 0.0);
	frag_pos2 = shadow2 * tpos - vec4(0.0, 0.0, 0.001, 0.0);
	
	//Texture coordinates
	vec2 t = texCoord.xy * 16.0;
	tc = vec4(fract(t), floor(t));

	//Lighting stuff	
	sun_light_color = clamp(dot(sun_dir, normal), 0.0, 1.0) * sun_color;
	light_color = lightColor;
}

