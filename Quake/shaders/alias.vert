#include "shader_defs.glsl"
#include "alias_instance_buffer.glsl"

layout(std430, binding=2) restrict readonly buffer PoseBuffer
{
	uvec2 PackedPosNor[];
};

layout(std430, binding=3) restrict readonly buffer UVBuffer
{
	vec2 TexCoords[];
};

struct Pose
{
	vec3 pos;
	vec3 nor;
};

Pose GetPose(uint index)
{
	uvec2 data = PackedPosNor[index + gl_VertexID];
	return Pose(vec3((data.xxx >> uvec3(0, 8, 16)) & 255), unpackSnorm4x8(data.y).xyz);
}

float r_avertexnormal_dot(vec3 vertexnormal, vec3 dir) // from MH 
{
	float d = dot(vertexnormal, dir);
	// wtf - this reproduces anorm_dots within as reasonable a degree of tolerance as the >= 0 case
	if (d < 0.0)
		return 1.0 + d * (13.0 / 44.0);
	else
		return 1.0 + d;
}

@if MODE == ALIASSHADER_NOPERSP
	layout(location=0) noperspective out vec2 out_texcoord;
@else
	layout(location=0) out vec2 out_texcoord;
@endif
layout(location=1) out vec4 out_color;
layout(location=2) out float out_fogdist;

void main()
{
	InstanceData inst = instances[gl_InstanceID];
	out_texcoord = TexCoords[gl_VertexID];
	Pose pose1 = GetPose(inst.Pose1);
	Pose pose2 = GetPose(inst.Pose2);
	vec3 lerpedVert = mix(pose1.pos, pose2.pos, inst.Blend);
	gl_Position = inst.MVP * vec4(lerpedVert, 1.0);
	out_fogdist = gl_Position.w;
	vec3 shadevector;
	shadevector[0] = cos(inst.ShadeAngle);
	shadevector[1] = sin(inst.ShadeAngle);
	shadevector[2] = 1.0;
	shadevector = normalize(shadevector);
	float dot1 = r_avertexnormal_dot(pose1.nor, shadevector);
	float dot2 = r_avertexnormal_dot(pose2.nor, shadevector);
	out_color = clamp(inst.LightColor * vec4(vec3(mix(dot1, dot2, inst.Blend)), 1.0), 0.0, 1.0);
	uint overbright = floatBitsToUint(Fog.w) >> 31;
	out_color.rgb = ldexp(out_color.rgb, ivec3(overbright));
}
