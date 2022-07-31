#include "shader_defs.glsl"
@if BINDLESS
	@extension GL_ARB_bindless_texture : require
@else
	layout(binding=0) uniform sampler2D Tex;
@endif

@include "framedata_buffer.glsl"
@include "noise_functions.glsl"

layout(location=0) flat in float in_alpha;
layout(location=1) in vec2 in_uv;
layout(location=2) in float in_fogdist;
@if BINDLESS
	layout(location=3) flat in uvec2 in_sampler;
@endif

layout(location=0) out vec4 out_fragcolor;

void main()
{
	vec2 uv = in_uv * 2.0 + 0.125 * sin(in_uv.yx * (3.14159265 * 2.0) + Time);
@if BINDLESS
	sampler2D Tex = sampler2D(in_sampler);
@endif
	vec4 result = texture(Tex, uv);
	result.rgb = ApplyFog(result.rgb, in_fogdist);
	result.a *= in_alpha;
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
