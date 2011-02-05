#ifdef GL_ES
precision highp float;
#endif

attribute vec3 pos;
attribute vec2 texCoord;
attribute vec3 normal;
attribute vec3 lightColor;

uniform mat4 proj;
uniform mat4 view;

//Sunlight parameters
uniform vec3 sun_dir;
uniform vec3 sun_color;

varying vec4 tc;
varying vec3 light_color;

void main(void)
{
	gl_Position = proj * view * vec4(pos, 1);
	
	vec2 t = texCoord.xy * 16.0;
	tc = vec4(fract(t), floor(t));
	
	float sun_value = dot(sun_dir, normal);
	
	if(sun_value < 0.0)
	{
		sun_value = 0.0;
	}
	
	light_color = lightColor + sun_value * sun_color;
}

