#include "shader_defs.glsl"
#include "framedata_buffer.glsl"

layout(location=0) in vec4 in_pos;
layout(location=1) in vec2 in_uv;

layout(location=0) out vec2 out_uv;
layout(location=1) out float out_fogdist;

void main()
{
	gl_Position = ViewProj * in_pos;
	out_fogdist = gl_Position.w;
	out_uv = in_uv;
}
