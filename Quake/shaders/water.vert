#include "shader_defs.glsl"
#include "bindless_vertex_header.glsl"
#include "framedata_buffer.glsl"
#include "world_calldata_buffer.glsl"
#include "world_instancedata_buffer.glsl"
#include "world_vertex_buffer.glsl"

layout(location=0) flat out float out_alpha;
layout(location=1) out vec2 out_uv;
layout(location=2) out float out_fogdist;
@if BINDLESS
	layout(location=3) flat out uvec2 out_sampler;
@endif

void main()
{
	Call call = call_data[DRAW_ID];
	int instance_id = GET_INSTANCE_ID(call);
	Instance instance = instance_data[instance_id];
	gl_Position = ViewProj * vec4(Transform(in_pos, instance), 1.0);
	out_uv = in_uv.xy;
	out_fogdist = gl_Position.w;
	out_alpha = instance.alpha < 0.0 ? call.wateralpha : instance.alpha;
@if BINDLESS
	out_sampler = call.txhandle;
@endif
}
