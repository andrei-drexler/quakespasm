#include "shader_defs.glsl"
#include "bindless_vertex_header.glsl"
#include "framedata_buffer.glsl"
#include "world_calldata_buffer.glsl"
#include "world_instancedata_buffer.glsl"
#include "world_vertex_buffer.glsl"

layout(location=0) out vec3 out_dir;
@if BINDLESS
	layout(location=1) flat out uvec4 out_samplers;
@endif

void main()
{
	Call call = call_data[DRAW_ID];
	int instance_id = GET_INSTANCE_ID(call);
	Instance instance = instance_data[instance_id];
	vec3 pos = Transform(in_pos, instance);
	gl_Position = ViewProj * vec4(pos, 1.0);
	out_dir = pos - EyePos;
	out_dir.z *= 3.0; // flatten the sphere
@if BINDLESS
	out_samplers.xy = call.txhandle;
	out_samplers.zw = call.fbhandle;
@endif
}
