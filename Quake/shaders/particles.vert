#include "framedata_buffer.glsl"

layout(location=0) in vec3 in_pos;
layout(location=1) in vec4 in_color;

layout(location=0) out vec2 out_uv;
layout(location=1) out vec4 out_color;
layout(location=2) out float out_fogdist;

layout(location=0) uniform vec2 ProjScale;

void main()
{
	// figure the current corner: (-1, -1), (-1, 1), (1, -1) or (1, 1)
	uvec2 flipsign = uvec2(gl_VertexID, gl_VertexID >> 1) << 31;
	vec2 corner = uintBitsToFloat(floatBitsToUint(-1.0) ^ flipsign);

	// project the center of the particle
	gl_Position = ViewProj * vec4(in_pos, 1.0);

	// hack a scale up to keep particles from disappearing
	float depthscale = max(1.0 + gl_Position.w * 0.004, 1.08);

	// perform the billboarding
	gl_Position.xy += ProjScale * uintBitsToFloat(floatBitsToUint(vec2(depthscale)) ^ flipsign);

	out_fogdist = gl_Position.w;
	out_uv = corner * 0.25 + 0.25; // remap corner to uv range
	out_color = in_color;
}
