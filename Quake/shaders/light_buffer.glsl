#define MAX_LIGHTS MAX_DLIGHTS

struct Light
{
	vec3	origin;
	float	radius;
	vec3	color;
	float	minlight;
};

layout(std430, binding=0) restrict readonly buffer LightBuffer
{
	float	LightStyles[MAX_LIGHTSTYLES];
	Light	Lights[];
};

float GetLightStyle(int index)
{
	return index < MAX_LIGHTSTYLES ? LightStyles[index] : 1.0;
}

