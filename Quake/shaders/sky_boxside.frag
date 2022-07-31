#include "shader_defs.glsl"
layout(binding=0) uniform sampler2D Tex;

#include "noise_functions.glsl"

layout(location=2) uniform vec4 Fog;

layout(location=0) in vec3 in_dir;
layout(location=1) in vec2 in_uv;

layout(location=0) out vec4 out_fragcolor;

void main()
{
	out_fragcolor = texture(Tex, in_uv);
	out_fragcolor.rgb = mix(out_fragcolor.rgb, Fog.rgb, Fog.w);
@if DITHER
	out_fragcolor.rgb = sqrt(out_fragcolor.rgb);
	out_fragcolor.rgb += SCREEN_SPACE_NOISE() * PAL_NOISESCALE;
	out_fragcolor.rgb *= out_fragcolor.rgb;
@endif
}
