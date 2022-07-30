struct InstanceData
{
	mat4	MVP;
	vec4	LightColor; // xyz=LightColor w=Alpha
	float	ShadeAngle;
	float	Blend;
	int		Pose1;
	int		Pose2;
};

layout(std430, binding=1) restrict readonly buffer InstanceBuffer
{
	vec4	Fog;
	InstanceData instances[];
};
