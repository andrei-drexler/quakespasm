#include "shader_defs.glsl"
#include "bindless_vertex_header.glsl"
#include "framedata_buffer.glsl"
#include "world_calldata_buffer.glsl"
#include "world_instancedata_buffer.glsl"
#include "world_vertex_buffer.glsl"

layout(location=0) out vec3 out_dir;

void main()
{
	Call call = call_data[DRAW_ID];
	int instance_id = GET_INSTANCE_ID(call);
	Instance instance = instance_data[instance_id];
	vec3 pos = Transform(in_pos, instance);
	gl_Position = ViewProj * vec4(pos, 1.0);
	out_dir.x = -(pos.y - EyePos.y);
	out_dir.y =  (pos.z - EyePos.z);
	out_dir.z =  (pos.x - EyePos.x);
}
