#include "bindless_vertex_header.glsl"
#include "framedata_buffer.glsl"
#include "light_buffer.glsl"
#include "world_calldata_buffer.glsl"
#include "world_instancedata_buffer.glsl"
#include "world_vertex_buffer.glsl"

layout(location=0) flat out uint out_flags;
layout(location=1) flat out float out_alpha;
layout(location=2) out vec3 out_pos;
@if MODE == WORLDSHADER_ALPHATEST
	layout(location=3) centroid out vec2 out_uv;
@else
	layout(location=3) out vec2 out_uv;
@endif
layout(location=4) centroid out vec2 out_lmuv;
layout(location=5) out float out_depth;
layout(location=6) noperspective out vec2 out_coord;
layout(location=7) flat out vec4 out_styles;
layout(location=8) flat out float out_lmofs;
@if BINDLESS
	layout(location=9) flat out uvec4 out_samplers;
@endif

void main()
{
	Call call = call_data[DRAW_ID];
	int instance_id = GET_INSTANCE_ID(call);
	Instance instance = instance_data[instance_id];
	out_pos = Transform(in_pos, instance);
	gl_Position = ViewProj * vec4(out_pos, 1.0);
@if REVERSED_Z
	const float ZBIAS = -1./1024;
@else
	const float ZBIAS =	 1./1024;
@endif
	if ((call.flags & CF_USE_POLYGON_OFFSET) != 0u)
		gl_Position.z += ZBIAS;
	out_uv = in_uv.xy;
	out_lmuv = in_uv.zw;
	out_depth = gl_Position.w;
	out_coord = (gl_Position.xy / gl_Position.w * 0.5 + 0.5) * vec2(LIGHT_TILES_X, LIGHT_TILES_Y);
	out_flags = call.flags;
@if MODE == WORLDSHADER_WATER
	out_alpha = instance.alpha < 0.0 ? call.wateralpha : instance.alpha;
@else
	out_alpha = instance.alpha < 0.0 ? 1.0 : instance.alpha;
@endif
	out_styles.x = GetLightStyle(in_styles.x);
	if (in_styles.y == 255)
		out_styles.yzw = vec3(-1.);
	else if (in_styles.z == 255)
		out_styles.yzw = vec3(GetLightStyle(in_styles.y), -1., -1.);
	else
		out_styles.yzw = vec3
		(
			GetLightStyle(in_styles.y),
			GetLightStyle(in_styles.z),
			GetLightStyle(in_styles.w)
		);
	if ((call.flags & CF_NOLIGHTMAP) != 0u)
		out_styles.xy = vec2(1., -1.);
	out_lmofs = in_lmofs;
@if BINDLESS
	out_samplers.xy = call.txhandle;
	if ((call.flags & CF_USE_FULLBRIGHT) != 0u)
		out_samplers.zw = call.fbhandle;
	else
		out_samplers.zw = out_samplers.xy;
@endif
}
