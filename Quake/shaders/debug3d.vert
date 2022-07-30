#include "framedata_buffer.glsl"

layout(location=0) in vec4 in_pos;
layout(location=1) in vec4 in_color;

layout(location=0) out vec4 out_color;

void main()
{
	gl_Position = ViewProj * in_pos;
	out_color = in_color;
}
