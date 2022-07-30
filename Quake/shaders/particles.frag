#include "framedata_buffer.glsl"
#include "noise_functions.glsl"

layout(binding=0) uniform sampler2D Tex;

layout(location=0) in vec2 in_uv;
layout(location=1) in vec4 in_color;
layout(location=2) in float in_fogdist;

layout(location=0) out vec4 out_fragcolor;

void main()
{
	vec4 result = texture(Tex, in_uv);
	result *= in_color;
	result.rgb = ApplyFog(result.rgb, in_fogdist);
	out_fragcolor = result;
@if DITHER
	if (Fog.w > 0.)
	{
		out_fragcolor.rgb = sqrt(out_fragcolor.rgb);
		out_fragcolor.rgb += SCREEN_SPACE_NOISE() * PAL_NOISESCALE;
		out_fragcolor.rgb *= out_fragcolor.rgb;
	}
@endif
}
