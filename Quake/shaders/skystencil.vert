#include "shader_defs.glsl"
#include "bindless_vertex_header.glsl"
#include "framedata_buffer.glsl"
#include "world_calldata_buffer.glsl"
#include "world_instancedata_buffer.glsl"
#include "world_vertex_buffer.glsl"

void main()
{
	Call call = call_data[DRAW_ID];
	int instance_id = GET_INSTANCE_ID(call);
	Instance instance = instance_data[instance_id];
	gl_Position = ViewProj * vec4(Transform(in_pos, instance), 1.0);
}
