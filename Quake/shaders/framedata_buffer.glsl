layout(std140, binding=0) uniform FrameDataUBO
{
	mat4	ViewProj;
	vec4	Fog;
	vec4	SkyFog;
	vec3	EyePos;
	float	Time;
	float	ZLogScale;
	float	ZLogBias;
	uint	NumLights;
};

vec3 ApplyFog(vec3 clr, float dist)
{
	dist *= Fog.w;
	float fog = exp2(-dist * dist);
	fog = clamp(fog, 0.0, 1.0);
	return mix(Fog.rgb, clr, fog);
}
