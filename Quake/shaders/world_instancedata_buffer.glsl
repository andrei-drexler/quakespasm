struct Instance
{
	vec4	mat[3];
	float	alpha;
};

layout(std430, binding=2) restrict readonly buffer InstanceBuffer
{
	Instance instance_data[];
};

vec3 Transform(vec3 p, Instance instance)
{
	mat4x3 world = transpose(mat3x4(instance.mat[0], instance.mat[1], instance.mat[2]));
	return mat3(world[0], world[1], world[2]) * p + world[3];
}
